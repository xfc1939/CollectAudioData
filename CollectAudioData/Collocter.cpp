#include "stdafx.h"
#include "Collocter.h"
#include "define.h"
#include "Utils.h"
#include <initguid.h>
#include <mmdeviceapi.h>
#include <AudioClient.h>
#include <functiondiscoverykeys_devpkey.h>

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

#define REFTIMES_PER_SEC  (10000000)

#define EXIT_ON_ERROR(hres) \
	if ( FAILED(hres)) { printf("hres is %d,\t%d",hres, __LINE__);break;}

#define SAFE_RELEASE(punk)	\
		if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

Collocter::Collocter()
	: pCb(NULL)
	, bStop_(false)
	, pThread_(NULL)
{
}


Collocter::~Collocter()
{
	if (pThread_) {
		bStop_ = true;
		pThread_->join();
		delete pThread_;
	}
	SAFE_RELEASE(pCaptureClient_);
	SAFE_RELEASE(pAudioClient_);
}

bool Collocter::Init(bool isInputDevice, int channels, int samplesRate, int wBitsPerSamples, CollocterCallBack *cb, std::string GUID)
{
	if (cb == NULL) {
		ERROR_OUTPUT("cb is nullptr");
		return false;
	}
	pCb = cb;
	bool ret = true;
	HRESULT hr = S_OK;

	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDeviceCollection *pCollection = NULL;
	SAFE_RELEASE(pCaptureClient_);
	SAFE_RELEASE(pAudioClient_);

	IMMDevice *pEndpoint = NULL;
	
	while (true) {
		hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void **)&pEnumerator);
		EXIT_ON_ERROR(hr);

		if (GUID.empty()) {
			hr = pEnumerator->GetDefaultAudioEndpoint(isInputDevice ? eCapture : eRender, isInputDevice ? eCommunications : eConsole, &pEndpoint);
			EXIT_ON_ERROR(hr);
		} else {
			hr = pEnumerator->EnumAudioEndpoints(isInputDevice ? eCapture : eRender, DEVICE_STATE_ACTIVE, &pCollection);
			EXIT_ON_ERROR(hr);

			UINT count;
			hr = pCollection->GetCount(&count);
			EXIT_ON_ERROR(hr);

			if (count == 0) {
				ERROR_OUTPUT("no endpoint found \n");
				break;
			}

			LPWSTR guid = NULL;
			for (ULONG i = 0; i < count; ++i) {
				hr = pCollection->Item(i, &pEndpoint);
				EXIT_ON_ERROR(hr);
					
				hr = pEndpoint->GetId(&guid);
				EXIT_ON_ERROR(hr);

				std::wstring wGuid(guid);
				if (Utils::wstring2string(wGuid) == GUID) {
					break;
				}

				SAFE_RELEASE(pEndpoint);
			}
			EXIT_ON_ERROR(hr);
			CoTaskMemFree(guid);

			if (pEndpoint == NULL) {
				ret = false;
				break;
			}
		}

		hr = pEndpoint->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&pAudioClient_);
		EXIT_ON_ERROR(hr);

		WAVEFORMATEX *pwfx = NULL;
		hr = pAudioClient_->GetMixFormat(&pwfx);
		EXIT_ON_ERROR(hr);

		PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);

		//adjust wave format
		//if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat))
		{
			pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

			if (isInputDevice) {
				pwfx->nSamplesPerSec = samplesRate;
			}

			pEx->Samples.wValidBitsPerSample = wBitsPerSamples;
			pwfx->wBitsPerSample = wBitsPerSamples;
			pwfx->nChannels = channels;
			pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
			pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
		}

		WAVEFORMATEX *pwfxCloseMath = NULL;
		pAudioClient_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, pwfx, &pwfxCloseMath);
		EXIT_ON_ERROR(hr);

		WAVEFORMATEX *tmpwfx = pwfxCloseMath ? pwfxCloseMath : pwfx;

		printf("format is %d, %d, %d\n", tmpwfx->nChannels, tmpwfx->nSamplesPerSec, tmpwfx->wBitsPerSample);
		
		DWORD flag = isInputDevice ? 0 : AUDCLNT_STREAMFLAGS_LOOPBACK;

		hr = pAudioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, flag, REFTIMES_PER_SEC, 0, tmpwfx, NULL);
		EXIT_ON_ERROR(hr);

		if (pCb) {
			pCb->setAudioFormat(tmpwfx->nChannels, tmpwfx->nSamplesPerSec, tmpwfx->wBitsPerSample);
		}

		UINT32 bufferFrameCount;
		hr = pAudioClient_->GetBufferSize(&bufferFrameCount);
		EXIT_ON_ERROR(hr);

		hr = pAudioClient_->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient_);
		EXIT_ON_ERROR(hr);

		hnsActualDuration_ = (double)REFTIMES_PER_SEC *
			bufferFrameCount / pwfx->nSamplesPerSec;
		CoTaskMemFree(pwfx);
		CoTaskMemFree(pwfxCloseMath);
		break;
	}

	if (FAILED(hr)) {
		ret = false;
	}

	if (!ret) {
		SAFE_RELEASE(pCaptureClient_);
		SAFE_RELEASE(pAudioClient_);
	}
	SAFE_RELEASE(pEnumerator);
	SAFE_RELEASE(pCollection);
	SAFE_RELEASE(pEndpoint);
	return ret;
}

void Collocter::Start()
{
	bStop_ = false;
	if (pThread_) {
		delete pThread_;
	}
	pThread_ = new std::thread(&Collocter::run, this);
}

void Collocter::Stop()
{
	bStop_ = true;
	if (pThread_) {
		pThread_->join();
		delete pThread_;
		pThread_ = NULL;
	}
}

void Collocter::run()
{
	printf("run start\n");
	if (!pAudioClient_ || !pCaptureClient_) {
		return;
	}
	printf("run stop\n");

	BYTE *pData = NULL;
	DWORD flags;
	HRESULT hr = S_OK;
	UINT32 packetLength = 0;
	UINT32 numFramesAvailable;

	hr = pAudioClient_->Start();
	if (FAILED(hr)) {
		bStop_ = true;
	}

	while (bStop_ == false) {
		Sleep(hnsActualDuration_ / REFTIMES_PER_SEC / 2);
		hr = pCaptureClient_->GetNextPacketSize(&packetLength);
		EXIT_ON_ERROR(hr);
		while (packetLength != 0) {
			hr = pCaptureClient_->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);

			EXIT_ON_ERROR(hr)
			if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
			{
				pData = NULL;  // Tell CopyData to write silence.
			}

			// Copy the available capture data to the audio sink.
			if (pData && pCb) {
				pCb->pushData(pData, numFramesAvailable);
			}

			hr = pCaptureClient_->ReleaseBuffer(numFramesAvailable);
			EXIT_ON_ERROR(hr)

			hr = pCaptureClient_->GetNextPacketSize(&packetLength);
			EXIT_ON_ERROR(hr)
		}
		EXIT_ON_ERROR(hr);
	}
	hr = pAudioClient_->Stop();
	SAFE_RELEASE(pAudioClient_);
	SAFE_RELEASE(pCaptureClient_);
}

