all:
	gcc ping_pong.c -O3 -o ping_pong -std=c11
	gcc ping_pong.c -O3 -o ping_pong_prefetch -std=c11 -DPREFETCH
	gcc ping_pong.c -O3 -o ping_pong_lfence -std=c11 -DLFENCE
	gcc ping_pong.c -O3 -o ping_pong_both -std=c11 -DLFENCE -DPREFETCH
