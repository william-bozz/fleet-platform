#pragma once
#include "civetweb.h"

int handle_api_stats_summary(struct mg_connection *conn, void *cbdata);
int handle_api_stats_fuel_daily(struct mg_connection *conn, void *cbdata);
int handle_api_stats_km_daily(struct mg_connection *conn, void *cbdata);
int handle_api_stats_pay_daily(struct mg_connection *conn, void *cbdata);
int handle_api_stats_profit_summary(struct mg_connection *conn, void *cbdata);
