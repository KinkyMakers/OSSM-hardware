#!/bin/sh

# This extract all lines with C-comments // as plain text
# Places all C-lines in quotes

gawk '
BEGIN {
	in_c_header = 1
	in_code = 0
	quote_code = 0
}

/#include/ { next }
(NF != 0) && (in_c_header == 1) { next }
/\/\/ *clang-format/ { next }

{ in_c_header = 0 }

(NF == 0) {
	if (in_code == 1) {
		print("```")
	}
	in_code = 0
	quote_code = 0
	next
}

/\/\// {
	if (in_code == 1) {
		print("```")
	}

	gsub("[ \t]*// ?","")
	print
	in_code = 0
	quote_code = 1
	next
}

(quote_code == 1) {
	print("```cpp")
	in_code = 1
	quote_code = 0
}

in_code == 1 {
	gsub("inline ","")
	print
}
' ../../src/FastAccelStepper.h >../doc/FastAccelStepper_API.md
