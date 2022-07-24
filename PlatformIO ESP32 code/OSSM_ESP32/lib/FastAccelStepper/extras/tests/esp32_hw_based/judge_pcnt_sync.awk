BEGIN {
	pass = 1
}

/^M1:/ {
	print
	api = substr($2,2)
	pcnt = substr($3,2,length($3)-2)
	if (pcnt < 0) {
		if (api > 0) {
			while (pcnt < 0) {
				pcnt += 65536
			}
		}
	}
	api = api % 65536
	if ((api-pcnt > 1) || (pcnt-api > 1)) {
		print api, pcnt
		pass = 0
		print "FAIL HERE ^^^"
	}
}
/^>> M1:/ {
	print
	api = substr($3,2)
	api = api % 65536
	pcnt = substr($4,2,length($4)-2)
	if (pcnt < 0) {
	    pcnt += 65536
	}
	if (api != pcnt) {
		print api, pcnt
		pass = 0
		print "FAIL HERE ^^^"
	}
}

END {
	if (pass) {
		print "PASS"
	}
	else {
		print "FAIL"
	}
}
