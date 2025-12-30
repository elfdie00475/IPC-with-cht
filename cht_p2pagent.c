#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cht_ipc_client.h"

int main(void)
{
    stCamStatusByIdReq testReq;
    stCamStatusByIdRep testRep;

    int rc = cht_ipc_getCamStatusById(&testReq, &testRep);

    return 0;
}
