BEGIN {
	ref = 16*100
	dump_all = 0
}

dump_all == 1{print}

/^\$var wire 1/ {
	names[++name_i] = $5
	sym[$4] = $5
	to_sym[$5] = $4
	period_hl_hl[$4] = 0
	period_lh_lh[$4] = 0
	time_h[$4] = 0
	time_l[$4] = 0
	transition_l_h[$4] = 0
	transition_h_l[$4] = 0
	cnt_l_h[$4] = 0
	cnt_h_l[$4] = 0
	max_time_h[$4] = 0
	sum_time_h[$4] = 0
	state[$4] = "X"
}

/^#/ { time = substr($1,2) + 0 }

/^1.$/ {
	s = substr($1,2)
	if(!SILENCE) printf("%s=1  ", sym[s])
	if (state[s] == 0) {
		# transition L->H
		cnt_l_h[s]++
		last = transition_l_h[s]
		transition_l_h[s] = time
		if (last > 0) {
			period_lh_lh[s] = time - last
		}
		last = transition_h_l[s]
		if (last > 0) {
			time_l[s] = time - last
		}

		if (sym[s] ~ /Step/) {
			channel = substr(sym[s],5)
			if(!SILENCE) printf("%s: ", channel)
			dir = "Dir" channel
			if (dir in to_sym) {
				dir_sym = to_sym[dir]
				if (state[dir_sym] == 0) {
					position[channel]--
				}
				else {
					position[channel]++
				}
				if(!SILENCE) printf("position=%d ",position[channel])
			}
		    if(!SILENCE) printf("period=%.1fus high time=%.1fus",
				  period_lh_lh[s]/ref,time_h[s]/ref)
		}
	}
	if(!SILENCE) printf("\n")
	state[s] = 1
}
/^0.$/ {
	s = substr($1,2)
	if(!SILENCE) printf("%s=0  ", sym[s])
	if (state[s] == 1) {
		# transition H->L
		cnt_h_l[s]++
		last = transition_h_l[s]
		transition_h_l[s] = time
		if (last > 0) {
			period_hl_hl[s] = time - last
		}
		last = transition_l_h[s]
		if (last > 0) {
			time_h[s] = time - last
		}
		if (sym[s] ~ /FillISR/) {
			if(!SILENCE) printf("period=%.1fus ", period_lh_lh[s]/ref)
		}
		h_time = time_h[s]/ref
		if(!SILENCE) printf("high time=%.1fus", h_time)
		sum_time_h[s] += h_time
		if (h_time > max_time_h[s]) {
			max_time_h[s] = h_time
		}
	}
	if(!SILENCE) printf("\n")
	state[s] = 0
}

END {
	n = asort(names)
	for (i = 1;i <= n;i++) {
		name = names[i]
		s = to_sym[name]
		info = sprintf("%8s: %8d*L->H, %8d*H->L",name,cnt_l_h[s],cnt_h_l[s])
		if (name ~ /Step/) {
			info = sprintf("%s, Max High=%dus Total High=%dus", info, max_time_h[s], sum_time_h[s])
		}
		if(!SILENCE) print(info)
		if (name !~ /ISR/) {
		    print(info) >"result.txt"
	    }
	}
	channels["A"]=1
	channels["B"]=1
	channels["C"]=1
	for (ch in channels) {
		if (ch in position) {
			info = sprintf("Position[%s]=%d\n",ch,position[ch])
			if(!SILENCE) print(info)
			print(info) >"result.txt"
		}
	}

	for (i = 1;i <= n;i++) {
		name = names[i]
		s = to_sym[name]
		if (max_time_h[s] > 0) {
			info = sprintf("Time in %s  max=%d us, total=%d us\n",name,max_time_h[s], sum_time_h[s])
			if(!SILENCE) print(info)
			print(info) >"result.txt"
		}
	}
}

