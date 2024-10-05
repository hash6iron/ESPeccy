#include "BuildDate.h"
#include "commit_info.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

const char* getShortBuildDate() {
    return COMMIT_DATE;
}
