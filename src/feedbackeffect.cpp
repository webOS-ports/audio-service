/* @@@LICENSE
*
* Copyright (c) 2013-2015 Simon Busch <morphis@gravedo.de>
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

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "feedbackeffect.h"
#include "audioservice.h"

#define SAMPLE_PATH		"/usr/share/systemsounds"

static GSList *sample_list = NULL;

FeedbackEffect::FeedbackEffect(AudioService *service, const std::string& name, const std::string& sink, bool play) :
    mService(service),
    mName(name),
    mSink(sink),
    mPlay(play)
{
    if (sample_list == NULL)
        sample_list = g_slist_alloc();
}

FeedbackEffect::~FeedbackEffect()
{
    if (mFd > 0)
        close(mFd);
}

void FeedbackEffect::run(FeedbackEffectResultCallback callback)
{
    mCallback = callback;

    if (mName.length() == 0) {
        finish(false);
        return;
    }
}

void FeedbackEffect::finish(bool success)
{
    if (mCallback)
        mCallback(success);
}

void FeedbackEffect::play_sample()
{
    pa_operation *op;
    pa_proplist *proplist;
    const char *sink = mSink.c_str();

    if (!mPlay) {
        finish(true);
        return;
    }

    if (!sink)
        sink = mService->default_sink_name();

    if (!sink) {
        finish(false);
        return;
    }

    /* make sure we're running as event to enable ducking */
    proplist = pa_proplist_new();
    pa_proplist_setf(proplist, PA_PROP_MEDIA_ROLE, "event");

    op = pa_context_play_sample_with_proplist(mService->context(), mName.c_str(),
                                              sink, PA_VOLUME_NORM, proplist,
                                              [] (pa_context *c, uint32_t idx, void *user_data) {
        FeedbackEffect *effect = static_cast<FeedbackEffect*>(user_data);

        if (idx == PA_INVALID_INDEX) {
            effect->finish(false);
            return;
        }

        effect->finish(true);

    }, this);

    if (op)
        pa_operation_unref(op);

    finish(true);
}

void FeedbackEffect::preload_sample()
{
    struct stat st;
    pa_sample_spec spec;
    char *sample_path;

    if (g_slist_find(sample_list, mName.c_str())) {
        play_sample();
        return;
    }

    sample_path = g_strdup_printf("%s/%s.pcm", SAMPLE_PATH, mName.c_str());

    if (stat(sample_path, &st) != 0) {
        g_free(sample_path);
        finish(false);
        return;
    }

    mSampleLength = st.st_size;

    spec.format = PA_SAMPLE_S16LE;
    spec.rate = 44100;
    spec.channels = 1;

    mFd = open(sample_path, O_RDONLY);
    if (mFd < 0){
        g_free(sample_path);
        finish(false);
        return;
    }

    mSampleStream = pa_stream_new(mService->context(), mName.c_str(), &spec, NULL);
    if (!mSampleStream){
        g_free(sample_path);
        finish(false);
        return;
    }

    pa_stream_set_state_callback(mSampleStream, [](pa_stream *stream, void *user_data) {
        FeedbackEffect *effect = static_cast<FeedbackEffect*>(user_data);

        switch (pa_stream_get_state(stream)) {
        case PA_STREAM_CREATING:
        case PA_STREAM_READY:
            return;
        case PA_STREAM_TERMINATED:
            g_message("Successfully uploaded sample %s to pulseaudio", effect->mName.c_str());
            sample_list = g_slist_append(sample_list, g_strdup(effect->mName.c_str()));
            effect->play_sample();
            break;
        case PA_STREAM_FAILED:
        default:
            g_warning("Failed to upload sample %s", effect->mName.c_str());
            effect->finish(false);
            break;
        }
    }, this);

    pa_stream_set_write_callback(mSampleStream, [](pa_stream *stream, size_t length, void *user_data) {
        FeedbackEffect *effect = static_cast<FeedbackEffect*>(user_data);
        void *buffer;
        ssize_t bread;

        buffer = pa_xmalloc(effect->mSampleLength);

        bread = read(effect->mFd, buffer, effect->mSampleLength);
        effect->mStreamWritten += bread;

        pa_stream_write(stream, buffer, bread, pa_xfree, 0, PA_SEEK_RELATIVE);

        if (effect->mStreamWritten == effect->mSampleLength) {
            pa_stream_set_write_callback(stream, NULL, NULL);
            pa_stream_finish_upload(stream);
        }
    }, this);

    pa_stream_connect_upload(mSampleStream, mSampleLength);

    g_free(sample_path);
}
