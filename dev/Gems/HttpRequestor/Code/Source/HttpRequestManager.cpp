/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#include "HttpRequestor_precompiled.h"

// The AWS Native SDK AWSAllocator triggers a warning due to accessing members of std::allocator directly.
// AWSAllocator.h(70): warning C4996: 'std::allocator<T>::pointer': warning STL4010: Various members of std::allocator are deprecated in C++17.
// Use std::allocator_traits instead of accessing these members directly.
// You can define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING or _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS to acknowledge that you have received this warning.
AZ_PUSH_DISABLE_WARNING(4251 4996, "-Wunknown-warning-option")
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/client/ClientConfiguration.h>
AZ_POP_DISABLE_WARNING

#include <AWSNativeSDKInit/AWSNativeSDKInit.h>
#include <AzCore/std/string/conversions.h>
#include "HttpRequestManager.h"

namespace HttpRequestor
{
    const char* Manager::s_loggingName = "GemHttpRequestManager";

    Manager::Manager()
    {
        AZStd::thread_desc desc;
        desc.m_name = s_loggingName;
        desc.m_cpuId = AFFINITY_MASK_USERTHREADS;
        m_runThread = true;
        // Shutdown will be handled by the InitializationManager - no need to call in the destructor
        AWSNativeSDKInit::InitializationManager::InitAwsApi();
        auto function = AZStd::bind(&Manager::ThreadFunction, this);
        m_thread = AZStd::thread(function, &desc);
    }

    Manager::~Manager()
    {
        // NativeSDK Shutdown does not need to be called here - will be taken care of by the InitializationManager
        m_runThread = false;
        m_requestConditionVar.notify_all();
        if (m_thread.joinable())
        {
            m_thread.join();
        }

    }

    void Manager::AddRequest(Parameters && httpRequestParameters)
    {
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_requestMutex);
            m_requestsToHandle.push(AZStd::move(httpRequestParameters));
        }
        m_requestConditionVar.notify_all();
    }

    void Manager::AddTextRequest(TextParameters && httpTextRequestParameters)
    {
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_requestMutex);
            m_textRequestsToHandle.push(AZStd::move(httpTextRequestParameters));
        }
        m_requestConditionVar.notify_all();
    }

    void Manager::ThreadFunction()
    {
        // Run the thread as long as directed
        while (m_runThread)
        {
            HandleRequestBatch();
        }
    }

    void Manager::HandleRequestBatch()
    {
        // Lock mutex and wait for work to be signalled via the condition variable
        AZStd::unique_lock<AZStd::mutex> lock(m_requestMutex);
        m_requestConditionVar.wait(lock, [&] { return !m_runThread || !m_requestsToHandle.empty() || !m_textRequestsToHandle.empty(); });

        // Swap queues
        AZStd::queue<Parameters> requestsToHandle;
        requestsToHandle.swap(m_requestsToHandle);

        AZStd::queue<TextParameters> textRequestsToHandle;
        textRequestsToHandle.swap(m_textRequestsToHandle);

        // Release lock
        lock.unlock();

        // Handle requests
        while (!requestsToHandle.empty())
        {
            HandleRequest(requestsToHandle.front());
            requestsToHandle.pop();
        }

        while (!textRequestsToHandle.empty())
        {
            HandleTextRequest(textRequestsToHandle.front());
            textRequestsToHandle.pop();
        }
    }

    void Manager::HandleRequest(const Parameters& httpRequestParameters)
    {
        std::shared_ptr<Aws::Http::HttpClient> httpClient = Aws::Http::CreateHttpClient(Aws::Client::ClientConfiguration());

        auto httpRequest = Aws::Http::CreateHttpRequest(httpRequestParameters.GetURI(), httpRequestParameters.GetMethod(), Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);

        for (const auto & it : httpRequestParameters.GetHeaders())
        {
            httpRequest->SetHeaderValue(it.first.c_str(), it.second.c_str());
        }

        if( httpRequestParameters.GetBodyStream() != nullptr)
        {
            httpRequest->AddContentBody(httpRequestParameters.GetBodyStream());
            httpRequest->SetContentLength(AZStd::to_string(httpRequestParameters.GetBodyStream()->str().length()).c_str());
        }
        
        auto httpResponse = httpClient->MakeRequest(httpRequest);

        if (!httpResponse)
        {
            httpRequestParameters.GetCallback()(Aws::Utils::Json::JsonValue(), Aws::Http::HttpResponseCode::INTERNAL_SERVER_ERROR);
            return;
        }

        if (httpResponse->GetResponseCode() != Aws::Http::HttpResponseCode::OK)
        {
            httpRequestParameters.GetCallback()(Aws::Utils::Json::JsonValue(), httpResponse->GetResponseCode());
            return;
        }

        Aws::Utils::Json::JsonValue json(httpResponse->GetResponseBody());
        if (json.WasParseSuccessful())
        {
            httpRequestParameters.GetCallback()(AZStd::move(json), httpResponse->GetResponseCode());
        }
        else
        {
            httpRequestParameters.GetCallback()(Aws::Utils::Json::JsonValue(), Aws::Http::HttpResponseCode::INTERNAL_SERVER_ERROR);
        }
    }

    void Manager::HandleTextRequest(const TextParameters & httpRequestParameters)
    {
        std::shared_ptr<Aws::Http::HttpClient> httpClient = Aws::Http::CreateHttpClient(Aws::Client::ClientConfiguration());

        auto httpRequest = Aws::Http::CreateHttpRequest(httpRequestParameters.GetURI(), httpRequestParameters.GetMethod(), Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);
        
        for (const auto & it : httpRequestParameters.GetHeaders())
        {
            httpRequest->SetHeaderValue(it.first.c_str(), it.second.c_str());
        }

        if (httpRequestParameters.GetBodyStream() != nullptr)
        {
            httpRequest->AddContentBody(httpRequestParameters.GetBodyStream());
        }

        auto httpResponse = httpClient->MakeRequest(httpRequest);

        if (!httpResponse)
        {
            httpRequestParameters.GetCallback()(AZStd::string(), Aws::Http::HttpResponseCode::INTERNAL_SERVER_ERROR);
            return;
        }

        if (httpResponse->GetResponseCode() != Aws::Http::HttpResponseCode::OK)
        {
            httpRequestParameters.GetCallback()(AZStd::string(), httpResponse->GetResponseCode());
            return;
        }

        // load up the raw output into a string
        // TODO(aaj): it feels like there should be some limit maybe 1 MB?
        std::istreambuf_iterator<char> eos;
        AZStd::string data(std::istreambuf_iterator<char>(httpResponse->GetResponseBody()), eos);
        httpRequestParameters.GetCallback()(AZStd::move(data), httpResponse->GetResponseCode());
    }
}