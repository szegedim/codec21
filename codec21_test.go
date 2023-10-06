package codec21

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

func testCODEC21Clean(t *testing.T) {
	// Technically we could use a random path in /tmp but this is easier to debug.
	_ = os.RemoveAll("./testpath/testout")
	_ = os.Mkdir("./testpath/testout", 0700)
}

// Runs all test modules. Ratio can be verified comparing each c21 file to testout_raw.c21
func TestCODEC21Modules(t *testing.T) {
	testCODEC21Clean(t)
	TestCODEC21Raw(t)
	TestCODEC21Masked(t)
	TestCODEC21LinearRegressionMethod(t)
	TestCODEC21LookupBit1(t)
	TestCODEC21LookupBit2(t)
	TestCODEC21LookupBit4(t)
}

func TestCODEC21EndToEnd(t *testing.T) {
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
			fil, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.c21", postfix))
			defer func() { _ = fil.Close() }()
			wr := bufio.NewWriter(fil)

			current := NewImage(img)
			err := Compress21(wr, &current, referencePtr)

			err = wr.Flush()
			if err != nil {
				return
			}
		}
		referencePtr = verifyImage(img, postfix, referencePtr)
	}
}

func TestCODEC21Raw(t *testing.T) {
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
		c21, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.c21", postfix))
		defer func() { _ = c21.Close() }()
		wr := bufio.NewWriter(c21)

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

func TestCODEC21Masked(t *testing.T) {
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

		fil, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.c21", postfix))
		defer func() { _ = fil.Close() }()
		wr := bufio.NewWriter(fil)

		cimg := NewImage(img)
		cursor := image.Point{X: (*img).Bounds().Min.X, Y: (*img).Bounds().Min.Y}

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

func TestCODEC21LinearRegressionMethod(t *testing.T) {
	img, _, err := readImage()
	if err != nil {
		t.Errorf("%s", os.Environ())
		t.Fatal(err)
	}

	postfix := "_linear"
	reference := NewImageRGBA(image.NewRGBA((*img).Bounds()))
	referencePtr := &reference

	{
		c21, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.c21", postfix))
		defer func() { _ = c21.Close() }()
		wr := bufio.NewWriter(c21)

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

func TestCODEC21LookupBit1(t *testing.T) {
	testLookupTableMethod(t, 1, 128)
}

func TestCODEC21LookupBit2(t *testing.T) {
	testLookupTableMethod(t, 2, 128)
}

func TestCODEC21LookupBit4(t *testing.T) {
	testLookupTableMethod(t, 4, 128)
}

func testLookupTableMethod(t *testing.T, bits uint8, size uint8) {
	img, _, err := readImage()
	if err != nil {
		t.Errorf("%s", os.Environ())
		t.Fatal(err)
	}

	postfix := fmt.Sprintf("_lut%v", bits)
	reference := NewImageRGBA(image.NewRGBA((*img).Bounds()))
	referencePtr := &reference

	{
		c21, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.c21", postfix))
		defer func() { _ = c21.Close() }()
		wr := bufio.NewWriter(c21)

		c21Image := NewImage(img)
		cursor := image.Point{X: (*img).Bounds().Min.X, Y: (*img).Bounds().Min.Y}

		for cursor.Y < c21Image.Bounds().Max.Y {
			buf, _, newRow, err := CompressNewLine(&c21Image, referencePtr, cursor)
			if err == nil && buf != nil {
				cursor = newRow
				var n int
				n, err = wr.Write(buf)
				if err != nil || n != len(buf) {
					t.Fatal(err)
				}
				continue
			}

			chunk := c21Image.Bounds().Intersect(image.Rectangle{Min: cursor, Max: cursor.Add(image.Point{int(size), 1})})
			buf, n, err := CompressLUT(&c21Image, chunk, bits)
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
	pngFile, err := os.Open("./testpath/benchmark.png")
	if err != nil {
		return nil, "", err
	}
	defer func() { _ = pngFile.Close() }()
	img, msg, err1 := image.Decode(bufio.NewReader(pngFile))
	return &img, msg, err1
}

func verifyImage(img *image.Image, postfix string, reference *Image) (current *Image) {
	fd, _ := os.Open(fmt.Sprintf("testpath/testout/testout%v.c21", postfix))
	decompressed, _ := Decompress((*img).Bounds(), bufio.NewReader(fd), reference)
	decompressedImage := Image(decompressed)
	// TODO compare
	pngFile, _ := os.Create(fmt.Sprintf("testpath/testout/testout%v.png", postfix))
	res := bufio.NewWriter(pngFile)
	defer func() { _ = pngFile.Close() }()
	_ = png.Encode(res, decompressed.base)
	_ = res.Flush()
	current = &decompressedImage
	return
}
