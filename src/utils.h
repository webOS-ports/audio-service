/* @@@LICENSE
*
* Copyright (c) 2012 Simon Busch <morphis@gravedo.de>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#ifndef UTILS_H_
#define UTILS_H_

#include <glib.h>
#include <luna-service2/lunaservice.h>

struct luna_service_req_data {
	LSHandle *handle;
	LSMessage *message;
	bool subscribed;
	void *user_data;
};

static inline struct luna_service_req_data *luna_service_req_data_new(LSHandle *handle, LSMessage *message)
{
	struct luna_service_req_data *req;

	req = g_new0(struct luna_service_req_data, 1);
	req->handle = handle;
	req->message = message;
	req->subscribed = false;

	LSMessageRef(req->message);

	return req;
}

static inline void luna_service_req_data_free(struct luna_service_req_data *req)
{
	if (!req)
		return;

	if (req->message)
		LSMessageUnref(req->message);

	g_free(req);
}

struct cb_data {
	void *cb;
	void *data;
	void *user;
};

static inline struct cb_data *cb_data_new(void *cb, void *data)
{
	struct cb_data *ret;

	ret = g_new0(struct cb_data, 1);
	ret->cb = cb;
	ret->data = data;
	ret->user = NULL;

	return ret;
}

#endif

// vim:ts=4:sw=4:noexpandtab
