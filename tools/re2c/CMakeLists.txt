cmake_minimum_required( VERSION 2.6 )
project( re2c )

add_executable( re2c
	actions.cc
	code.cc
	dfa.cc
	main.cc
	mbo_getopt.cc
	parser.cc
	scanner.cc
	substr.cc
	translate.cc )
