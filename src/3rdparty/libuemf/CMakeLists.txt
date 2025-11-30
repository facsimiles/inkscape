add_library(uemf_LIB STATIC)

target_sources(uemf_LIB PRIVATE
	uemf.c
	uemf_endian.c
	uemf_print.c
	uemf_safe.c
	uemf_utf.c
	uwmf.c
	uwmf_endian.c
	uwmf_print.c
	upmf.c
	upmf_print.c

	uemf.h
	uemf_endian.h
	uemf_print.h
	uemf_safe.h
	uemf_utf.h
	uwmf.h
	uwmf_endian.h
	uwmf_print.h
	upmf.h
	upmf_print.h
)

target_link_libraries(uemf_LIB PUBLIC Iconv::Iconv)
target_include_directories(uemf_LIB PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
