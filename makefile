all:	BenchRedisHLL Benchwyhll
BenchRedisHLL:	BenchRedisHLL.c
	gcc BenchRedisHLL.c hyperloglog.c -o BenchRedisHLL -Ofast -fpermissive -w -lm -march=native
Benchwyhll:	Benchwyhll.c wyhll.h
	gcc Benchwyhll.c -o Benchwyhll -Ofast -Wall -lm -march=native

