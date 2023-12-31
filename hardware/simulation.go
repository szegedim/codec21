package main

import (
	"fmt"
	"sort"
)

// Licensed with Creative Commons CC0.
// Patent and trademark rights reserved. See license.md for details.
//
// To the extent possible under law, the author(s) have dedicated all copyright and related and
// neighboring rights to this software to the public domain worldwide.
// This software is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

type GreenEndian struct {
	value int
	code  int
	order int
}

func main() {
	list := make([]GreenEndian, 0)
	for i := 0; i < 256; i++ {
		n := 0
		j := i
		for j > 0 {
			n = n + (j & 1)
			j = j >> 1
		}
		list = append(list, GreenEndian{value: 0, code: i, order: n})
	}
	sort.Slice(list, func(i, j int) bool {
		return list[i].order<<8+list[i].code < list[j].order<<8+list[j].code
	})
	for value, x := range list {
		x.value = value
		fmt.Printf("%2d %08b\n", x.value, x.code)
	}
}
