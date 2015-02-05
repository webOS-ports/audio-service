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

#include "luna_service_utils.h"

void luna_service_message_reply_custom_error(LSHandle *handle, LSMessage *message, const char *error_text)
{
	bool ret;
	LSError lserror;
	char *payload;

	LSErrorInit(&lserror);

	payload = g_strdup_printf("{\"returnValue\":false, \"errorText\":\"%s\"}", error_text);

	ret = LSMessageReply(handle, message, payload, &lserror);
	if (!ret) {
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	g_free(payload);
}

void luna_service_message_reply_error_unknown(LSHandle *handle, LSMessage *message)
{
	luna_service_message_reply_custom_error(handle, message, "Unknown Error.");
}

void luna_service_message_reply_error_bad_json(LSHandle *handle, LSMessage *message)
{
	luna_service_message_reply_custom_error(handle, message, "Malformed json.");
}

void luna_service_message_reply_error_invalid_params(LSHandle *handle, LSMessage *message)
{
	luna_service_message_reply_custom_error(handle, message, "Invalid parameters.");
}

void luna_service_message_reply_error_not_implemented(LSHandle *handle, LSMessage *message)
{
	luna_service_message_reply_custom_error(handle, message, "Not implemented.");
}

void luna_service_message_reply_error_internal(LSHandle *handle, LSMessage *message)
{
	luna_service_message_reply_custom_error(handle, message, "Internal error.");
}

void luna_service_message_reply_success(LSHandle *handle, LSMessage *message)
{
	bool ret;
	LSError lserror;

	LSErrorInit(&lserror);

	ret = LSMessageReply(handle, message, "{\"returnValue\":true}", &lserror);
	if (!ret) {
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}
}

jvalue_ref luna_service_message_parse_and_validate(const char *payload)
{
	jschema_ref input_schema = NULL;
	jvalue_ref parsed_obj = NULL;
	JSchemaInfo schema_info;

	input_schema = jschema_parse(j_cstr_to_buffer("{}"), DOMOPT_NOOPT, NULL);
	jschema_info_init(&schema_info, input_schema, NULL, NULL);

	parsed_obj = jdom_parse(j_cstr_to_buffer(payload), DOMOPT_NOOPT, &schema_info);

	jschema_release(&input_schema);
	if (jis_null(parsed_obj))
		return NULL;

	return parsed_obj;
}

bool luna_service_message_get_boolean(jvalue_ref parsed_obj, const char *name, bool default_value)
{
	jvalue_ref boolean_obj;
	bool value;

	if (!jobject_get_exists(parsed_obj, j_str_to_buffer(name, strlen(name)), &boolean_obj) ||
		!jis_boolean(boolean_obj))
		return default_value;

	jboolean_get(boolean_obj, &value);

	return value;
}

char* luna_service_message_get_string(jvalue_ref parsed_obj, const char *name, const char *default_value)
{
	jvalue_ref string_obj = NULL;
	raw_buffer string_buf;

	if (!jobject_get_exists(parsed_obj, j_str_to_buffer(name, strlen(name)), &string_obj) ||
		!jis_string(string_obj))
		return g_strdup(default_value);

	string_buf = jstring_get(string_obj);

	return g_strdup(string_buf.m_str);
}

bool luna_service_message_validate_and_send(LSHandle *handle, LSMessage *message, jvalue_ref reply_obj)
{
	jschema_ref response_schema = NULL;
	LSError lserror;
	bool success = true;

	LSErrorInit(&lserror);

	response_schema = jschema_parse (j_cstr_to_buffer("{}"), DOMOPT_NOOPT, NULL);
	if(!response_schema) {
		luna_service_message_reply_error_internal(handle, message);
		return false;
	}

	if (!LSMessageReply(handle, message,
					jvalue_tostring(reply_obj, response_schema), &lserror)) {
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
		success = false;
	}

	jschema_release(&response_schema);

	return success;
}

bool luna_service_check_for_subscription_and_process(LSHandle *handle, LSMessage *message)
{
	LSError lserror;
	bool subscribed = false;

	LSErrorInit(&lserror);

	if (LSMessageIsSubscription(message)) {
		if (!LSSubscriptionProcess(handle, message, &subscribed, &lserror)) {
			LSErrorPrint(&lserror, stderr);
			LSErrorFree(&lserror);
		}
	}

	return subscribed;
}

void luna_service_post_subscription(LSHandle *handle, const char *path, const char *method, jvalue_ref reply_obj)
{
	jschema_ref response_schema = NULL;
	LSError lserror;

	LSErrorInit(&lserror);

	response_schema = jschema_parse (j_cstr_to_buffer("{}"), DOMOPT_NOOPT, NULL);
	if(!response_schema)
		goto cleanup;

	if (!LSSubscriptionPost(handle, path, method,
						jvalue_tostring(reply_obj, response_schema), &lserror)) {
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

cleanup:
	if (response_schema)
		jschema_release(&response_schema);
}

// vim:ts=4:sw=4:noexpandtab
