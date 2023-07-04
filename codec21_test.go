package codec20

import (
	"bufio"
	"fmt"
	"image"
	"image/png"
	"os"
	"testing"

	_ "image/png"
)

// Licensed with Creative Commons CC0.
// Patent and trademark rights reserved. See license.md for details.
//
// To the extent possible under law, the author(s) have dedicated all copyright and related and
// neighboring rights to this software to the public domain worldwide.
// This software is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

func testCodec20Clean(t *testing.T) {
	_ = os.RemoveAll("./testpath/testout")
	_ = os.Mkdir("./testpath/testout", 0700)
}

func TestCodec20Parts(t *testing.T) {
	testCodec20Clean(t)
	TestCodec20Raw(t)
	TestCodec20Masked(t)
	TestCodec20Linear(t)
	TestCodec20LookupBit1(t)
	TestCodec20LookupBit2(t)
	TestCodec20LookupBit4(t)
}

func TestCodec20Full(t *testing.T) {
	img, _, err := readImage()
	if err != nil {
		t.Errorf("%s", os.Environ())
		t.Fatal(err)
	}

	referenceRGBA := image.NewRGBA((*img).Bounds())
	reference := NewImageRGBA(referenceRGBA)
	referencePtr := &reference

	for i := 0; i < 10; i++ {
		postfix := fmt.Sprintf("_full_%vms", i*16)
		{
			fil, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.c20", postfix))
			defer fil.Close()
			wr := bufio.NewWriter(fil)

			current := NewImage(img)
			err := Compress20(wr, &current, referencePtr)

			err = wr.Flush()
			if err != nil {
				return
			}
		}
		referencePtr = verifyImage(img, postfix, referencePtr)
	}
}

func TestCodec20Raw(t *testing.T) {
	img, _, err := readImage()
	if err != nil {
		t.Errorf("%s", os.Environ())
		t.Fatal(err)
	}

	postfix := "_raw"
	referenceRGBA := image.NewRGBA((*img).Bounds())
	reference := NewImageRGBA(referenceRGBA)
	referencePtr := &reference

	{
		fil, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.c20", postfix))
		defer fil.Close()
		wr := bufio.NewWriter(fil)

		cimg := NewImage(img)
		cursor := image.Point{(*img).Bounds().Min.X, (*img).Bounds().Min.Y}

		for cursor.Y < cimg.Bounds().Max.Y {
			buf, _, newRow, err := CompressNewLine(&cimg, referencePtr, cursor)
			if err == nil && buf != nil {
				cursor = newRow
				var n int
				n, err = wr.Write(buf)
				if err != nil || n != len(buf) {
					t.Fatal(err)
				}
				continue
			}

			const maxLength = 24 // 24 raw true color bytes uncompressed
			buf, n, next, err := CompressRaw(&cimg, cursor, maxLength)
			if err == nil && n > 0 {
				cursor = next
				var nn int
				nn, err = wr.Write(buf)
				if err != nil || nn != len(buf) {
					t.Fatal(err)
				}
			}
		}
		err = wr.Flush()
		if err != nil {
			return
		}
	}

	referencePtr = verifyImage(img, postfix, referencePtr)
}

func TestCodec20Masked(t *testing.T) {
	img, _, err := readImage()
	if err != nil {
		t.Errorf("%s", os.Environ())
		t.Fatal(err)
	}

	referenceRGBA := image.NewRGBA((*img).Bounds())
	reference := NewImageRGBA(referenceRGBA)
	referencePtr := &reference

	for i := 0; i < 10; i++ {
		postfix := fmt.Sprintf("_quantized_%vms", i*16)

		fil, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.c20", postfix))
		defer fil.Close()
		wr := bufio.NewWriter(fil)

		cimg := NewImage(img)
		cursor := image.Point{(*img).Bounds().Min.X, (*img).Bounds().Min.Y}

		for cursor.Y < cimg.Bounds().Max.Y {
			buf, _, newRow, err := CompressNewLine(&cimg, referencePtr, cursor)
			if err == nil && buf != nil {
				cursor = newRow
				var n int
				n, err = wr.Write(buf)
				if err != nil || n != len(buf) {
					t.Fatal(err)
				}
				continue
			}

			buf, _, cursor1, err := CompressMasked(&cimg, referencePtr, cursor)
			if err == nil && buf != nil {
				cursor = cursor1
				var nn int
				nn, err = wr.Write(buf)
				if err != nil || nn != len(buf) {
					t.Fatal(err)
				}
			}
		}
		err = wr.Flush()
		if err != nil {
			t.Fatal(err)
		}

		referencePtr = verifyImage(img, postfix, referencePtr)
	}
}

func TestCodec20Linear(t *testing.T) {
	img, _, err := readImage()
	if err != nil {
		t.Errorf("%s", os.Environ())
		t.Fatal(err)
	}

	postfix := "_linear"
	reference := NewImageRGBA(image.NewRGBA((*img).Bounds()))
	referencePtr := &reference

	{
		fil, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.c20", postfix))
		defer fil.Close()
		wr := bufio.NewWriter(fil)

		cimg := NewImage(img)
		cursor := image.Point{(*img).Bounds().Min.X, (*img).Bounds().Min.Y}

		for cursor.Y < cimg.Bounds().Max.Y {
			buf, _, newRow, err := CompressNewLine(&cimg, referencePtr, cursor)
			if err == nil && buf != nil {
				cursor = newRow
				var n int
				n, err = wr.Write(buf)
				if err != nil || n != len(buf) {
					t.Fatal(err)
				}
				continue
			}

			buf, _, next, errl := CompressLinear(&cimg, cursor)
			if errl == nil {
				cursor = next
			}

			var nn int
			nn, err = wr.Write(buf)
			if err != nil || nn != len(buf) {
				t.Fatal(err)
			}
		}
		_ = wr.Flush()
	}

	referencePtr = verifyImage(img, postfix, referencePtr)
}

func TestCodec20LookupBit1(t *testing.T) {
	testLookup(t, 1, 128)
}

func TestCodec20LookupBit2(t *testing.T) {
	testLookup(t, 2, 128)
}

func TestCodec20LookupBit4(t *testing.T) {
	testLookup(t, 4, 128)
}

func testLookup(t *testing.T, bits uint8, size uint8) {
	img, _, err := readImage()
	if err != nil {
		t.Errorf("%s", os.Environ())
		t.Fatal(err)
	}

	postfix := fmt.Sprintf("_lut%v", bits)
	reference := NewImageRGBA(image.NewRGBA((*img).Bounds()))
	referencePtr := &reference

	{
		fil, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.c20", postfix))
		defer fil.Close()
		wr := bufio.NewWriter(fil)

		cimg := NewImage(img)
		cursor := image.Point{(*img).Bounds().Min.X, (*img).Bounds().Min.Y}

		for cursor.Y < cimg.Bounds().Max.Y {
			buf, _, newRow, err := CompressNewLine(&cimg, referencePtr, cursor)
			if err == nil && buf != nil {
				cursor = newRow
				var n int
				n, err = wr.Write(buf)
				if err != nil || n != len(buf) {
					t.Fatal(err)
				}
				continue
			}

			chunk := cimg.Bounds().Intersect(image.Rectangle{cursor, cursor.Add(image.Point{int(size), 1})})
			buf, n, err := CompressLUT(&cimg, chunk, bits)
			if err != nil {
				t.Fatal(err)
			}
			length := 2 + (1<<bits)*3 + (int(buf[0])*int(bits)+7)/8
			if length != len(buf) {
				t.Fatalf("length error %v %v", length, len(buf))
			}
			cursor.X = cursor.X + n
			var nn int
			nn, err = wr.Write(buf)
			if err != nil || nn != len(buf) {
				t.Fatal(err)
			}
		}
		_ = wr.Flush()
	}

	referencePtr = verifyImage(img, postfix, referencePtr)
}

func readImage() (*image.Image, string, error) {
	fimg, err := os.Open("./testpath/benchmark.png")
	if err != nil {
		return nil, "", err
	}
	defer fimg.Close()
	img, msg, err1 := image.Decode(bufio.NewReader(fimg))
	return &img, msg, err1
}

func verifyImage(img *image.Image, postfix string, reference *Image) (current *Image) {
	fd, _ := os.Open(fmt.Sprintf("testpath/testout/testout%v.c20", postfix))
	decompressed, _ := Decompress((*img).Bounds(), bufio.NewReader(fd), reference)
	decompressedImage := Image(decompressed)
	// TODO compare
	fpng, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.png", postfix))
	res := bufio.NewWriter(fpng)
	defer fpng.Close()
	_ = png.Encode(res, decompressed.base)
	_ = res.Flush()
	current = &decompressedImage
	return
}
