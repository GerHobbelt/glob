
#pragma once

#if defined(BUILD_MONOLITHIC)

#ifdef __cplusplus
extern "C" {
#endif

	extern int glob_standalone_main(int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif
