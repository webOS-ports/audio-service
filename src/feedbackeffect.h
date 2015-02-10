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

#ifndef FEEDBACKEFFECT_H
#define FEEDBACKEFFECT_H

#include <string>
#include <functional>

typedef std::function<void(bool)> FeedbackEffectResultCallback;

class AudioService;

class FeedbackEffect
{
public:
    FeedbackEffect(AudioService *service, const std::string name, const std::string sink, bool play);
    ~FeedbackEffect();

    void run(FeedbackEffectResultCallback callback);

private:
    AudioService *mService;
    std::string mName;
    std::string mSink;
    bool mPlay;
    pa_stream *mSampleStream;
    unsigned int mSampleLength;
    unsigned int mStreamWritten;
    int mFd;

    FeedbackEffectResultCallback mCallback;

    void preload_sample();
    void play_sample();
    void finish(bool success);

    static void preload_stream_write_cb(pa_stream *stream, size_t length, void *user_data);
    static void preload_stream_state_cb(pa_stream *stream, void *user_data);
};

#endif // FEEDBACKEFFECT_H
