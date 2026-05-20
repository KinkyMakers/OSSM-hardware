#ifndef TOPPRA_EXPORT_H
#define TOPPRA_EXPORT_H

// Hand-rolled stand-in for the file that CMake's generate_export_header()
// normally produces. We statically link toppra into the firmware so the
// import/export macros are intentional no-ops.

#define TOPPRA_EXPORT
#define TOPPRA_NO_EXPORT

#ifndef TOPPRA_DEPRECATED
#  define TOPPRA_DEPRECATED __attribute__((__deprecated__))
#endif

#ifndef TOPPRA_DEPRECATED_EXPORT
#  define TOPPRA_DEPRECATED_EXPORT TOPPRA_EXPORT TOPPRA_DEPRECATED
#endif

#ifndef TOPPRA_DEPRECATED_NO_EXPORT
#  define TOPPRA_DEPRECATED_NO_EXPORT TOPPRA_NO_EXPORT TOPPRA_DEPRECATED
#endif

#endif  // TOPPRA_EXPORT_H
