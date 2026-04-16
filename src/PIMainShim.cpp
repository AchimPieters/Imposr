// Adobe requires the SDK support file PIMain.c to be part of every plug-in.
// The official docs recommend adding the SDK's PIMain.c directly to the project.
// This shim keeps the repository cleaner by including it from the SDK installation.

extern "C" {
#include "PIMain.c"
}
