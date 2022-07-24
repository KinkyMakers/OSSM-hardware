BEGIN {
	ok = 1
	timing = 0
	if (DIR ~ /timing/) {
		timing = 1
	}
}

FNR == NR {
	# result.txt
	lines[FNR] = $0
}

FNR != NR {
	# expect.txt
	r = lines[FNR]
	e = $0

	r_test = r
	e_test = e

	if (timing == 0) {
		gsub(/[0-9 ]*us/,"", r_test)
		gsub(/[0-9 ]*us/,"", e_test)
	}
	if (e_test != r_test) {
		print("result:",r)
		print("expect:",e)
		ok = 0
	}
}

END {
	if (ok == 1) {
		print("PASS")
		print("PASS") > ".tested"
	}
}
