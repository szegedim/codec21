package codec21

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"image"
	"image/color"
	"io"
	"math/bits"
)

// Licensed with Creative Commons CC0.
// Patent and trademark rights reserved. See license.md for details.
//
// To the extent possible under law, the author(s) have dedicated all copyright and related and
// neighboring rights to this software to the public domain worldwide.
// This software is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

const TransparentMarker = uint8(0x00)
const Lookup1BitMarker = uint8(0x01)
const Lookup2BitMarker = uint8(0x02)
const Lookup4BitMarker = uint8(0x03)
const MaskBits76Marker = uint8(0x04)
const MaskBits54Marker = uint8(0x05)
const MaskBits32Marker = uint8(0x06)
const MaskBits10Marker = uint8(0x07)
const ImageRowMarker = uint8(0x0D)
const LinearMarker = uint8(0x0E)
const RawMarker = uint8(0x0F)

var nomatch = errors.New("nomatch")

func Compress20(out *bufio.Writer, current *Image, reference *Image) error {
	bounds := (*current).Bounds()
	cursor := image.Point{bounds.Min.X, bounds.Min.Y}

	for cursor.Y < bounds.Max.Y {
		buf, _, next, err := CompressChunk(current, reference, cursor)
		if err != nil {
			return err
		}
		var nn int
		nn, err = out.Write(buf)
		if err != nil || nn != len(buf) {
			return err
		}
		cursor = next
	}

	return nil
}

type Pixel struct {
	R uint8
	G uint8
	B uint8
}

func (p *Pixel) Full() uint32 {
	r := int32(p.R)
	g := int32(p.G)
	b := int32(p.B)
	return uint32(r + g + b)
}

type ShortPixel uint32

func (p *Pixel) Short() ShortPixel {
	r := int32(p.R)
	g := int32(p.G)
	b := int32(p.B)
	return ShortPixel(r>>4<<8 + g>>4<<4 + b>>4)
}

func (p *Pixel) LooksLike(q *Pixel) bool {
	return p.Diff(q) < 4*3
}

func (p *Pixel) Diff(q *Pixel) uint32 {
	r := int32(p.R) - int32(q.R)
	if r < 0 {
		r = -r
	}
	g := int32(p.G) - int32(q.G)
	if g < 0 {
		g = -g
	}
	b := int32(p.B) - int32(q.B)
	if b < 0 {
		b = -b
	}
	return uint32(r + g + b)
}

type Image interface {
	At(x, y int) (r uint8, g uint8, b uint8)
	PixelAt(x, y int) *Pixel
	Set(x, y int, color color.RGBA)
	Bounds() image.Rectangle
	Image() *image.Image
}

type GoImage struct {
	base *image.Image
}

func NewImage(img *image.Image) Image {
	return &GoImage{img}
}

func (t *GoImage) Set(x, y int, color color.RGBA) {
}

func (t *GoImage) PixelAt(x, y int) *Pixel {
	r, g, b := t.At(x, y)
	return &Pixel{r, g, b}
}

func (t *GoImage) At(x, y int) (r uint8, g uint8, b uint8) {
	rw, gw, bw, _ := (*t.base).At(x, y).RGBA()
	return uint8(rw / 0x100), uint8(gw / 0x100), uint8(bw / 0x100)
}

func (t *GoImage) Bounds() image.Rectangle {
	return (*t.base).Bounds()
}

func (t *GoImage) Image() *image.Image {
	return t.base
}

type RGBAImage struct {
	base *image.RGBA
}

func NewImageRGBA(img *image.RGBA) Image {
	return &RGBAImage{img}
}

func NewImageRGBAPtr(img *image.RGBA) *RGBAImage {
	return &RGBAImage{img}
}

func (t *RGBAImage) Set(x, y int, color color.RGBA) {
	(*t.base).SetRGBA(x, y, color)
}

func (t *RGBAImage) PixelAt(x, y int) *Pixel {
	r, g, b := t.At(x, y)
	return &Pixel{r, g, b}
}

func (t *RGBAImage) At(x, y int) (r uint8, g uint8, b uint8) {
	rw, gw, bw, _ := (*t.base).At(x, y).RGBA()
	return uint8(rw / 0x100), uint8(gw / 0x100), uint8(bw / 0x100)
}

func (t *RGBAImage) Bounds() image.Rectangle {
	return (*t.base).Bounds()
}

func (t *RGBAImage) Image() *image.Image {
	return nil
}

func CompressChunk(current *Image, reference *Image, cursor image.Point) ([]byte, int, image.Point, error) {
	var buf *[]byte = nil
	var n int = 0
	var next image.Point
	var err error = nil
	var ratio = 1000

	row, nrow, crow, erow := CompressNewLine(current, reference, cursor)
	if erow == nil {
		return row, nrow, crow, nil
	}

	const lutsize = 128
	lut1, nlut1, clut1, elut1 := CompressLUTAtCursor(current, cursor, 1, lutsize)
	if elut1 == nil && ratio > len(lut1)*100/nlut1/3 {
		buf = &lut1
		n = nlut1
		next = clut1
		ratio = len((*buf)) * 100 / n / 3
	}

	lut2, nlut2, clut2, elut2 := CompressLUTAtCursor(current, cursor, 2, lutsize)
	if elut2 == nil && ratio > len(lut2)*100/nlut2/3 {
		buf = &lut2
		n = nlut2
		next = clut2
		ratio = len((*buf)) * 100 / n / 3
	}

	lut4, nlut4, clut4, elut4 := CompressLUTAtCursor(current, cursor, 4, lutsize)
	if elut4 == nil && ratio > len(lut4)*100/nlut4/3 {
		buf = &lut4
		n = nlut4
		next = clut4
		ratio = len((*buf)) * 100 / n / 3
	}

	lin, nlin, clin, elin := CompressLinear(current, cursor)
	if elin == nil && ratio > len(lin)*100/nlin/3 {
		buf = &lin
		n = nlin
		next = clin
		ratio = len((*buf)) * 100 / n / 3
	}

	raw4, nraw4, craw4, eraw4 := CompressRaw(current, cursor, 4)
	if eraw4 == nil && ratio > len(raw4)*100/nraw4/3 {
		buf = &raw4
		n = nraw4
		next = craw4
		ratio = len((*buf)) * 100 / n / 3
	}

	raw2, nraw2, craw2, eraw2 := CompressRaw(current, cursor, 2)
	if eraw2 == nil && ratio > len(raw2)*100/nraw2/3 {
		buf = &raw2
		n = nraw2
		next = craw2
		ratio = len((*buf)) * 100 / n / 3
	}

	raw1, nraw1, craw1, eraw1 := CompressRaw(current, cursor, 1)
	if eraw1 == nil && ratio > len(raw1)*100/nraw1/3 {
		buf = &raw1
		n = nraw1
		next = craw1
		ratio = len((*buf)) * 100 / n / 3
	}

	msk, nmsk, cmsk, emsk := CompressMasked(current, reference, cursor)
	if emsk == nil && ratio > len(msk)*100/nmsk/3 && buf == nil {
		buf = &msk
		n = nmsk
		next = cmsk
		ratio = len((*buf)) * 100 / n / 3
	} else if emsk == nil && (msk[1] == MaskBits10Marker || msk[1] == MaskBits32Marker || msk[1] == MaskBits54Marker || msk[1] == TransparentMarker) {
		buf = &msk
		n = nmsk
		next = cmsk
		ratio = len((*buf)) * 100 / n / 3
	}

	return *buf, n, next, err
}

func CompressNewLine(result *Image, reference *Image, cursor image.Point) ([]byte, int, image.Point, error) {
	bounds := (*result).Bounds().Intersect(image.Rect(cursor.X, cursor.Y, (*result).Bounds().Max.X, cursor.Y+1))
	if bounds.Empty() {
		cursor.X = (*result).Bounds().Min.X
		cursor.Y++
		return []byte{1, ImageRowMarker}, 0, cursor, nil
	}

	bounds = (*result).Bounds().Intersect(image.Rect((*result).Bounds().Min.X, cursor.Y, (*result).Bounds().Max.X, (*result).Bounds().Max.Y))
	n := uint8(0)
	var r, g, b uint8
	for y := bounds.Min.Y; y < bounds.Max.Y && n < 255; y++ {
		same := true
		for x := bounds.Min.X; x < bounds.Max.X; x++ {
			r0, g0, b0 := (*reference).At(x, y)
			reference := Pixel{r0, g0, b0}
			r1, g1, b1 := (*result).At(x, y)
			current := Pixel{r1, g1, b1}
			if reference != current {
				same = false
				break
			}
			(*result).Set(x, y, color.RGBA{r, g, b, 255})
		}
		if same {
			n++
		} else {
			break
		}
	}

	if n == 0 {
		return nil, 0, image.Point{}, fmt.Errorf("row not finished")
	}
	cursor.X = (*result).Bounds().Min.X
	cursor.Y = cursor.Y + int(n)
	return []byte{n, ImageRowMarker}, 0, cursor, nil
}

func DecompressNewLine(result *Image, reference *Image, cursor image.Point, reader *bufio.Reader) (int, image.Point, error) {
	buf, err := reader.Peek(2)
	if err != nil || buf[0] < 1 || buf[1] != ImageRowMarker {
		return 0, image.Point{}, fmt.Errorf("nomatch")
	}
	max := int(buf[0])
	_, err = reader.Discard(2)
	if err != nil {
		return 0, image.Point{}, fmt.Errorf("nomatch")
	}
	boundsCursor := (*result).Bounds().Intersect(image.Rectangle{image.Point{cursor.X, cursor.Y}, image.Point{cursor.X + 1, cursor.Y + 1}})

	var n int = 0
	if boundsCursor.Empty() {
		n++
	}

	bounds := (*result).Bounds().Intersect(image.Rectangle{image.Point{cursor.X, cursor.Y}, image.Point{(*result).Bounds().Max.X, (*result).Bounds().Max.Y}})
	var r, g, b uint8
	for y := bounds.Min.Y; y < bounds.Max.Y && n < max; y++ {
		for x := bounds.Min.X; x < bounds.Max.X; x++ {
			r, g, b = (*reference).At(x, y)
			(*result).Set(x, y, color.RGBA{r, g, b, 255})
		}
		n++
	}

	cursor.X = (*result).Bounds().Min.X
	cursor.Y = cursor.Y + n

	return n, cursor, err
}

func buildPaletteLinearComplexity(current *Image, bounds image.Rectangle, bits uint8) []Pixel {
	length := uint16(1 << bits)
	palette := make([]Pixel, length)
	i := 0
	n := 0
	for y := bounds.Min.Y; y < bounds.Max.Y; y++ {
		for x := bounds.Min.X; x < bounds.Max.X; x++ {
			r, g, b := (*current).At(x, y)
			pix := Pixel{r, g, b}
			candidate := true
			for j := 0; j < i; j++ {
				if palette[j].LooksLike(&pix) {
					candidate = false
					break
				}
			}
			if candidate {
				if i < int(length) {
					palette[i] = pix
					i++
					if i == int(length) {
						return palette
					}
				} else {
					break
				}
			}
			n++
		}
	}
	return palette
}

type Codec21LUTWriter struct {
	bits    uint8
	length  int
	palette []Pixel
	buf     bytes.Buffer
	idx     []uint8
}

func (t *Codec21LUTWriter) WriteBits(index int) error {
	if int(uint8(index)) != index {
		return fmt.Errorf("bad index")
	}
	valid := uint8(index)
	palettelen := uint16(1 << t.bits)
	mask := uint8(palettelen - 1)
	arraypos := t.length * int(t.bits) / 8
	shift := t.length * int(t.bits) % 8
	bufLen := t.buf.Len()
	if bufLen <= arraypos {
		t.buf.WriteByte(0)
	}
	t.buf.Bytes()[arraypos] = t.buf.Bytes()[arraypos] | ((valid & mask) << shift)
	t.idx = append(t.idx, valid)
	t.length++
	return nil
}

func (t *Codec21LUTWriter) Flush() ([]byte, error) {
	if t.length == 0 || t.length > 255 {
		return nil, fmt.Errorf("length error")
	}

	buf := bytes.Buffer{}
	wr := bufio.NewWriter(&buf)
	err := wr.WriteByte(uint8(t.length))
	if err != nil {
		return nil, err
	}

	var marker uint8
	if t.bits == 1 {
		marker = Lookup1BitMarker
	} else if t.bits == 2 {
		marker = Lookup2BitMarker
	} else if t.bits == 4 {
		marker = Lookup4BitMarker
	} else {
		return nil, fmt.Errorf("unsupported lookup bit %v", t.bits)
	}
	err = wr.WriteByte(marker)
	if err != nil {
		return nil, err
	}
	err = binary.Write(wr, binary.LittleEndian, t.palette)
	if err != nil {
		return nil, err
	}

	var nn int
	nn, err = wr.Write(t.buf.Bytes())
	if err != nil {
		return nil, err
	}
	if nn != len(t.buf.Bytes()) {
		return nil, fmt.Errorf("insufficient buffer")
	}
	err = wr.Flush()
	if err != nil {
		return nil, err
	}

	t.buf.Reset()
	t.palette = []Pixel{}
	t.length = 0

	return buf.Bytes(), nil
}

type Codec21LUTReader struct {
	base    *bufio.Reader
	bits    uint8
	pos     int
	length  int
	palette []Pixel
	pixels  []byte
	idx     []uint8
}

func (t *Codec21LUTReader) ReadIndex() (Pixel, error) {
	if t.pos >= t.length {
		buf, err := t.base.Peek(2)
		if err != nil || len(buf) < 2 {
			return Pixel{}, err
		}
		t.bits = 0
		palette := int(0)
		if buf[1] == Lookup1BitMarker {
			t.bits = 1
			palette = 2
		} else if buf[1] == Lookup2BitMarker {
			t.bits = 2
			palette = 4
		} else if buf[1] == Lookup4BitMarker {
			t.bits = 4
			palette = 16
		} else {
			return Pixel{}, nomatch
		}
		if buf[0] < 1 || t.bits == 0 {
			return Pixel{}, nomatch
		}

		length := 2 + palette*3 + (int(buf[0])*int(t.bits)+7)/8
		buf = make([]byte, length)
		var n int
		n, err = io.ReadFull(t.base, buf)
		if n != length {
			return Pixel{}, fmt.Errorf("incomplete stream")
		}
		buffer := bytes.NewBuffer(buf)
		reader := bufio.NewReader(buffer)

		_, err = reader.Discard(2)
		if err != nil {
			return Pixel{}, err
		}

		t.palette = make([]Pixel, palette)
		err = binary.Read(reader, binary.LittleEndian, t.palette)
		if err != nil {
			return Pixel{}, err
		}

		t.length = int(buf[0])
		indexLength := (t.length*int(t.bits) + 7) / 8
		t.pixels = make([]byte, indexLength)
		var nn int
		nn, err = io.ReadFull(reader, t.pixels)
		if err != nil || nn != len(t.pixels) {
			return Pixel{}, err
		}
		_, err = reader.ReadByte()
		if err == nil {
			return Pixel{}, fmt.Errorf("leaked byte")
		}
		t.pos = 0
	}

	palettelen := uint16(1 << t.bits)
	mask := uint8(palettelen - 1)
	arraypos := t.pos * int(t.bits) / 8
	shift := t.pos * int(t.bits) % 8
	idx := (t.pixels[arraypos] >> shift) & mask
	p := t.palette[idx]
	t.idx = append(t.idx, idx)
	t.pos++

	return p, nil
}

type BitStreamWriter struct {
	base    *bufio.Writer
	acc     uint8
	pos     int
	hasdata bool
}

func (t *BitStreamWriter) WriteBits(data uint8, nbits uint8, shift uint8) error {
	length := uint16(1 << nbits)
	mask := uint8(length - 1)

	data = (data >> shift) & mask
	t.hasdata = t.hasdata || data > 0
	t.acc = t.acc<<nbits | data
	npos := (t.pos + int(nbits)) % 8
	if npos < t.pos {
		err := t.base.WriteByte(t.acc)
		if err != nil {
			return err
		}
		if npos != 0 {
			return fmt.Errorf("overflow")
		}
		t.acc = 0
		npos = 0
	}
	t.pos = npos
	return nil
}

func (t *BitStreamWriter) flush() error {
	var err error
	if t.pos != 0 {
		t.acc = t.acc << (8 - t.pos)
		err = t.base.WriteByte(t.acc)
		if err != nil {
			return err
		}
	}
	err = t.base.Flush()
	if err != nil {
		return err
	}
	t.acc = 0
	t.pos = 0
	return nil
}

func (t *BitStreamWriter) Valid() bool {
	err := t.flush()
	return err == nil && t.hasdata
}

func CompressMasked(current *Image, reference *Image, cursor image.Point) ([]byte, int, image.Point, error) {
	const maxLength = 8 * 16 // 24 raw true color bytes compressed to 2 + 2 = 4 ~ 17%
	bounds := (*current).Bounds()
	chunk := bounds.Intersect(image.Rect(cursor.X, cursor.Y, cursor.X+maxLength, cursor.Y+1))
	fmt.Sprintf("%v\n", cursor)
	if chunk.Empty() {
		return nil, 0, cursor, fmt.Errorf("noop")
	}

	buf1000diff := bytes.Buffer{}
	buf1000diffwriter := BitStreamWriter{bufio.NewWriter(&buf1000diff), 0, 0, false}
	buf0100diff := bytes.Buffer{}
	buf0100diffwriter := BitStreamWriter{bufio.NewWriter(&buf0100diff), 0, 0, false}
	buf0010diff := bytes.Buffer{}
	buf0010diffwriter := BitStreamWriter{bufio.NewWriter(&buf0010diff), 0, 0, false}
	buf0001diff := bytes.Buffer{}
	buf0001diffwriter := BitStreamWriter{bufio.NewWriter(&buf0001diff), 0, 0, false}

	buf1000 := bytes.Buffer{}
	buf1000writer := BitStreamWriter{bufio.NewWriter(&buf1000), 0, 0, false}
	buf0100 := bytes.Buffer{}
	buf0100writer := BitStreamWriter{bufio.NewWriter(&buf0100), 0, 0, false}
	buf0010 := bytes.Buffer{}
	buf0010writer := BitStreamWriter{bufio.NewWriter(&buf0010), 0, 0, false}
	buf0001 := bytes.Buffer{}
	buf0001writer := BitStreamWriter{bufio.NewWriter(&buf0001), 0, 0, false}

	n := uint8(0)
	raw := false

	for y := chunk.Min.Y; y < chunk.Max.Y; y++ {
		for x := chunk.Min.X; x < chunk.Max.X; x++ {
			r0, g0, b0 := (*reference).At(x, y)
			r, g, b := (*current).At(x, y)

			if !raw && r >= r0 && b >= b0 && g >= g0 {
				// Send just a diff
				_ = buf1000diffwriter.WriteBits(r-r0, 2, 6)
				_ = buf1000diffwriter.WriteBits(g-g0, 2, 6)
				_ = buf1000diffwriter.WriteBits(b-b0, 2, 6)

				_ = buf0100diffwriter.WriteBits(r-r0, 2, 4)
				_ = buf0100diffwriter.WriteBits(g-g0, 2, 4)
				_ = buf0100diffwriter.WriteBits(b-b0, 2, 4)

				_ = buf0010diffwriter.WriteBits(r-r0, 2, 2)
				_ = buf0010diffwriter.WriteBits(g-g0, 2, 2)
				_ = buf0010diffwriter.WriteBits(b-b0, 2, 2)

				_ = buf0001diffwriter.WriteBits(r-r0, 2, 0)
				_ = buf0001diffwriter.WriteBits(g-g0, 2, 0)
				_ = buf0001diffwriter.WriteBits(b-b0, 2, 0)
			} else {
				raw = true
			}

			// Raw bits
			_ = buf1000writer.WriteBits(r, 2, 6)
			_ = buf1000writer.WriteBits(g, 2, 6)
			_ = buf1000writer.WriteBits(b, 2, 6)

			_ = buf0100writer.WriteBits(r, 2, 4)
			_ = buf0100writer.WriteBits(g, 2, 4)
			_ = buf0100writer.WriteBits(b, 2, 4)

			_ = buf0010writer.WriteBits(r, 2, 2)
			_ = buf0010writer.WriteBits(g, 2, 2)
			_ = buf0010writer.WriteBits(b, 2, 2)

			_ = buf0001writer.WriteBits(r, 2, 0)
			_ = buf0001writer.WriteBits(g, 2, 0)
			_ = buf0001writer.WriteBits(b, 2, 0)

			n++
		}
	}

	skip := uint8(0)
	mask := skip
	var max *bytes.Buffer
	if raw {
		if buf1000writer.Valid() {
			max = &buf1000
			mask = 8
		} else if buf0100writer.Valid() {
			max = &buf0100
			mask = 4
		} else if buf0010writer.Valid() {
			max = &buf0010
			mask = 2
		} else if buf0001writer.Valid() {
			max = &buf0001
			mask = 1
		} else {
			max = &buf1000
			mask = 1
		}
	} else {
		if buf1000diffwriter.Valid() {
			max = &buf1000diff
			mask = 8
		} else if buf0100diffwriter.Valid() {
			max = &buf0100diff
			mask = 4
		} else if buf0010diffwriter.Valid() {
			max = &buf0010diff
			mask = 2
		} else if buf0001diffwriter.Valid() {
			max = &buf0001diff
			mask = 1
		} else {
			mask = skip
		}
	}

	buf := bytes.Buffer{}
	marker := uint8(0xFF)
	if mask == 0b0000 {
		marker = TransparentMarker
	}
	if mask == 0b1000 {
		marker = MaskBits76Marker
	}
	if mask == 0b0100 {
		marker = MaskBits54Marker
	}
	if mask == 0b0010 {
		marker = MaskBits32Marker
	}
	if mask == 0b0001 {
		marker = MaskBits10Marker
	}
	if marker == 0xFF {
		return nil, 0, cursor, fmt.Errorf("nomask")
	}
	if mask != skip {
		buf.WriteByte(n)
		buf.WriteByte(marker)
		buf.Write(max.Bytes())
		cursor.X = cursor.X + int(n)
		return buf.Bytes(), int(n), cursor, nil
	} else {
		buf.WriteByte(n)
		buf.WriteByte(marker)
		cursor.X = cursor.X + int(n)
		return buf.Bytes(), int(n), cursor, nil
	}
}

type BitStreamReader struct {
	base *bufio.Reader
	acc  uint8
	num  int
}

func (t *BitStreamReader) ReadBits(nbits uint8, shift uint8) (uint8, error) {
	if t.num == 0 {
		b, err := t.base.ReadByte()
		if err != nil {
			return 0, err
		}
		t.acc = b
		t.num = 8
	}
	length := uint16(1 << nbits)
	mask := uint8(length - 1)

	ret := (t.acc >> (8 - nbits)) & mask
	t.acc = t.acc << nbits
	t.num = (t.num - int(nbits)) % 8

	return ret << shift, nil
}

func DecompressMasked(result *Image, reference *Image, cursor image.Point, reader *bufio.Reader) (int, error) {
	buf, err := reader.Peek(2)
	if err != nil {
		return 0, err
	}
	marker := buf[1] & 0x0F
	mask := uint8(0xFF)
	if marker == TransparentMarker {
		mask = 0b0000
	}
	if marker == MaskBits76Marker {
		mask = 0b1000
	}
	if marker == MaskBits54Marker {
		mask = 0b0100
	}
	if marker == MaskBits32Marker {
		mask = 0b0010
	}
	if marker == MaskBits10Marker {
		mask = 0b0001
	}

	if err != nil || buf[0] < 1 || mask == 0xFF {
		return 0, fmt.Errorf("nomatch")
	}
	_, err = reader.Discard(2)
	if err != nil {
		return 0, fmt.Errorf("nomatch")
	}
	length := uint32(buf[0])
	bounds := (*result).Bounds().Intersect(image.Rect(cursor.X, cursor.Y, cursor.X+int(length), cursor.Y+1))
	//mask := uint8(buf[1] & 0x0F)
	shift := uint8(bits.TrailingZeros(uint(mask)) * 2)
	skip := mask == 0
	bitCount := uint8(bits.OnesCount8(mask) * 2)
	rd := BitStreamReader{reader, 0, 0}
	chunk := (*result).Bounds().Intersect(image.Rect(bounds.Min.X, bounds.Min.Y, bounds.Min.X+int(length), bounds.Min.Y+1))

	n := int(0)

	var r, g, b uint8
	for y := chunk.Min.Y; y < chunk.Max.Y; y++ {
		for x := chunk.Min.X; x < chunk.Max.X; x++ {
			if skip {
				r, g, b = (*reference).At(x, y)
			} else {
				r, err = rd.ReadBits(bitCount, shift)
				if err != nil {
					return n, err
				}
				g, err = rd.ReadBits(bitCount, shift)
				if err != nil {
					return n, err
				}
				b, err = rd.ReadBits(bitCount, shift)
				if err != nil {
					return n, err
				}

				if bitCount+shift < 8 {
					// delta
					r0, g0, b0 := (*reference).At(x, y)
					r = r + r0
					g = g + g0
					b = b + b0
				}
			}

			(*result).Set(x, y, color.RGBA{r, g, b, 255})
			n++
			if uint32(n) >= length {
				break
			}
		}
		if uint32(n) >= length {
			break
		}
	}

	return n, err
}

func CompressRaw(current *Image, cursor image.Point, maxLength int) ([]byte, int, image.Point, error) {
	bounds := (*current).Bounds()
	chunk := bounds.Intersect(image.Rect(cursor.X, cursor.Y, cursor.X+maxLength, cursor.Y+1))
	if chunk.Empty() {
		return nil, 0, cursor, fmt.Errorf("empty")
	}

	buf := bytes.Buffer{}
	writer := bufio.NewWriter(&buf)
	buf.WriteByte(0)
	buf.WriteByte(RawMarker)

	n := uint8(0)

	var err error
	for y := chunk.Min.Y; y < chunk.Max.Y; y++ {
		for x := chunk.Min.X; x < chunk.Max.X; x++ {
			r, g, b := (*current).At(x, y)
			err = writer.WriteByte(r)
			if err != nil {
				return nil, 0, cursor, err
			}
			err = writer.WriteByte(g)
			if err != nil {
				return nil, 0, cursor, err
			}
			err = writer.WriteByte(b)
			if err != nil {
				return nil, 0, cursor, err
			}

			n++
		}
	}

	err = writer.Flush()
	if err != nil {
		return nil, 0, cursor, err
	}

	buf.Bytes()[0] = n

	return buf.Bytes(), int(n), image.Point{cursor.X + int(n), cursor.Y}, nil
}

func DecompressRaw(result *Image, cursor image.Point, reader *bufio.Reader) (int, error) {
	buf, err := reader.Peek(2)
	if err != nil || buf[0] < 1 || (buf[1]&0x0F) != RawMarker {
		return 0, fmt.Errorf("nomatch")
	}
	_, err = reader.Discard(2)
	if err != nil {
		return 0, fmt.Errorf("err discard")
	}
	length := uint32(buf[0])
	bounds := (*result).Bounds().Intersect(image.Rect(cursor.X, cursor.Y, cursor.X+int(length), cursor.Y+1))
	n := int(0)
	var r, g, b uint8
	for x := bounds.Min.X; x < bounds.Max.X; x++ {
		r, err = reader.ReadByte()
		if err != nil {
			return 0, fmt.Errorf("err read at (%v, %v)", x, cursor.Y)
		}
		g, err = reader.ReadByte()
		if err != nil {
			return 0, fmt.Errorf("err read at (%v, %v)", x, cursor.Y)
		}
		b, err = reader.ReadByte()
		if err != nil {
			return 0, fmt.Errorf("err read at (%v, %v)", x, cursor.Y)
		}
		(*result).Set(x, cursor.Y, color.RGBA{r, g, b, 255})
		n++
		if uint32(n) >= length {
			break
		}
	}

	return n, err
}

func CompressLinear(current *Image, cursor image.Point) ([]byte, int, image.Point, error) {
	const slopeLength = 8
	const maxLength = 240

	base := Pixel{0, 0, 0}
	if cursor.X-(*current).Bounds().Min.X >= 1 {
		r, g, b := (*current).At(cursor.X-1, cursor.Y)
		base = Pixel{r, g, b}
	}

	r0, g0, b0 := uint32(base.R)*0x100, uint32(base.G)*0x100, uint32(base.B)*0x100
	bounds := (*current).Bounds()
	slope := bounds.Intersect(image.Rect(cursor.X, cursor.Y, cursor.X+slopeLength, cursor.Y+1))
	if slope.Empty() {
		return nil, 0, cursor, fmt.Errorf("noslope")
	}
	r1b, g1b, b1b := (*current).At(slope.Max.X, slope.Min.Y)
	rslope, gslope, bslope := (uint32(r1b)*0x100-r0)/slopeLength, (uint32(g1b)*0x100-g0)/slopeLength, (uint32(b1b)*0x100-b0)/slopeLength

	chunk := bounds.Intersect(image.Rect(cursor.X, cursor.Y, cursor.X+maxLength, cursor.Y+1))

	n := uint8(0)
	finished := false
	for y := chunk.Min.Y; y < chunk.Max.Y; y++ {
		for x := chunk.Min.X; x < chunk.Max.X; x++ {
			distance := uint32(1 + x - chunk.Min.X)
			actual := (*current).PixelAt(x, y)
			expected := &Pixel{
				uint8((r0 + rslope*distance) / 0x100),
				uint8((g0 + gslope*distance) / 0x100),
				uint8((b0 + bslope*distance) / 0x100)}

			if !actual.LooksLike(expected) || n == 255 {
				finished = true
				break
			}
			base = *actual
			n++
		}
		if finished {
			break
		}
	}

	buf := bytes.Buffer{}
	if n == 0 {
		n = 1
		buf.WriteByte(n)
		buf.WriteByte(LinearMarker)
		base = *(*current).PixelAt(cursor.X, cursor.Y)
		buf.WriteByte(base.R)
		buf.WriteByte(base.G)
		buf.WriteByte(base.B)
	} else {
		buf.WriteByte(n)
		buf.WriteByte(LinearMarker)
		buf.WriteByte(base.R)
		buf.WriteByte(base.G)
		buf.WriteByte(base.B)
	}
	var err error = nil
	if n == 0 {
		err = fmt.Errorf("nonlinear")
	}
	return buf.Bytes(), int(n), image.Point{cursor.X + int(n), cursor.Y}, err
}

func DecompressLinear(result *Image, cursor image.Point, reader *bufio.Reader, base Pixel) (int, error) {
	buf, err := reader.Peek(5)
	if err != nil || buf[0] < 1 || buf[1] != LinearMarker {
		return 0, fmt.Errorf("nomatch")
	}
	length := uint32(buf[0])
	chunk := (*result).Bounds().Intersect(image.Rect(cursor.X, cursor.Y, cursor.X+int(length), cursor.Y+1))

	r0, g0, b0 := uint32(base.R)*0x100, uint32(base.G)*0x100, uint32(base.B)*0x100
	r1, g1, b1 := uint32(buf[2])*0x100, uint32(buf[3])*0x100, uint32(buf[4])*0x100

	n := int(0)
	for y := chunk.Min.Y; y < chunk.Max.Y; y++ {
		for x := chunk.Min.X; x < chunk.Max.X; x++ {
			weightMax := uint32(1 + x - chunk.Min.X)
			weightMin := uint32(chunk.Max.X - x - 1)
			p := &Pixel{
				uint8((r0*weightMin + r1*weightMax) / length / 0x100),
				uint8((g0*weightMin + g1*weightMax) / length / 0x100),
				uint8((b0*weightMin + b1*weightMax) / length / 0x100)}

			(*result).Set(x, y, color.RGBA{p.R, p.G, p.B, 255})
			n++
			if uint32(n) >= length {
				break
			}
		}
		if uint32(n) >= length {
			break
		}
	}

	_, err = reader.Discard(5)
	if err != nil {
		n = 0
	}

	return n, err
}

func CompressLUTAtCursor(current *Image, cursor image.Point, bits uint8, size uint8) ([]byte, int, image.Point, error) {
	bounds := (*current).Bounds().Intersect(image.Rectangle{cursor, cursor.Add(image.Point{int(size), 1})})
	buf, n, err := CompressLUT(current, bounds, bits)
	cursor.X = cursor.X + n
	return buf, n, cursor, err
}

func CompressLUT(current *Image, bounds image.Rectangle, bits uint8) ([]byte, int, error) {
	if bounds.Max.Y != bounds.Min.Y+1 {
		return nil, 0, fmt.Errorf("single line error")
	}

	writer := Codec21LUTWriter{bits: bits, length: 0, buf: bytes.Buffer{}, palette: nil}
	length := uint8(1 << bits)
	n := int(0)

	writer.palette = buildPaletteLinearComplexity(current, bounds, bits)

	for x := bounds.Min.X; x < bounds.Max.X; x++ {
		selected := int(-1)
		r, g, b := (*current).At(x, bounds.Min.Y)
		pix := &Pixel{r, g, b}
		for c := uint8(0); c < length; c++ {
			if writer.palette[c].LooksLike(pix) {
				selected = int(c)
				break
			}
		}
		err := writer.WriteBits(selected)
		if err == nil {
			n++
		} else {
			break
		}
	}

	buf, err := writer.Flush()
	if err != nil {
		return nil, 0, err
	}

	return buf, n, nil
}

func DecompressLUT(result *Image, cursor image.Point, buffered *bufio.Reader, bits uint8) (int, error) {
	buf, err := buffered.Peek(2)
	if err != nil {
		return 0, err
	}
	if buf[1] != Lookup1BitMarker && buf[1] != Lookup2BitMarker && buf[1] != Lookup4BitMarker {
		return 0, fmt.Errorf("nomatch")
	}

	codec := Codec21LUTReader{base: buffered, bits: 0, pos: 0, length: 0, pixels: nil, palette: nil}
	bounds := (*result).Bounds().Intersect(image.Rect(cursor.X, cursor.Y, (*result).Bounds().Max.X, cursor.Y+1))

	n := int(0)
	for x := bounds.Min.X; x < bounds.Max.X; x++ {
		var p Pixel
		p, err := codec.ReadIndex()
		if err == io.EOF || err == nomatch {
			break
		}
		if err != nil {
			return 0, err
		}
		(*result).Set(x, cursor.Y, color.RGBA{p.R, p.G, p.B, 255})
		n++
	}
	if codec.pos < codec.length {
		return 0, fmt.Errorf("stream overflow")
	}
	return n, nil
}

func Decompress(bounds image.Rectangle, reader *bufio.Reader, reference *Image) (current *RGBAImage, err error) {
	cursor := image.Point{X: bounds.Min.X, Y: bounds.Min.Y}

	result := image.NewRGBA(bounds)
	resultImage := NewImageRGBAPtr(result)
	interimImage := Image(resultImage)
	base := Pixel{0, 0, 0}

	for cursor.Y < bounds.Max.Y {
		n, cursor1, err := DecompressNewLine(&interimImage, reference, cursor, reader)
		if err == nil {
			cursor = cursor1
			base = Pixel{0, 0, 0}
			continue
		}
		n, err = DecompressRaw(&interimImage, cursor, reader)
		if err != nil {
			n, err = DecompressLUT(&interimImage, cursor, reader, 2)
			if err != nil {
				n, err = DecompressMasked(&interimImage, reference, cursor, reader)
				if err != nil {
					n, err = DecompressLinear(&interimImage, cursor, reader, base)
					if err != nil {
						// TODO return nil?
						return resultImage, err
					}
				}
			}
		}
		if n >= 1 {
			r, g, b := resultImage.At(cursor.X+n-1, cursor.Y)
			base = Pixel{r, g, b}
		}
		cursor.X = cursor.X + n
	}

	return resultImage, nil
}
