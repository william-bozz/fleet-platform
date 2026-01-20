#pragma once
#include "civetweb.h"

int handle_dashboard(struct mg_connection *conn, void *cbdata);

int handle_chart_fuel_liters_by_truck(struct mg_connection *conn, void *cbdata);
int handle_chart_fuel_cost_by_truck(struct mg_connection *conn, void *cbdata);
int handle_chart_km_by_truck(struct mg_connection *conn, void *cbdata);
int handle_chart_pay_by_driver(struct mg_connection *conn, void *cbdata);
