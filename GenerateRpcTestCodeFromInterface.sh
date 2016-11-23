#!/bin/bash

rpcgen -C -h ./TestShared/AcmeTesting.x > ./TestShared/AcmeTesting_onc.h
rpcgen -C -l ./TestShared/AcmeTesting.x > ./TestRpcClient/AcmeTesting_onc_clnt.c
rpcgen -C -m ./TestShared/AcmeTesting.x > ./IntegrationTests/AcmeTesting_onc_svc.c

