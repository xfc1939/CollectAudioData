#pragma once

#include <string>
#include <thread>
struct IAudioCaptureClient;
struct IAudioClient;

class CollocterCallBack {
public:
	CollocterCallBack() = default;
	virtual ~CollocterCallBack() = default;

public:
	virtual void pushData(void *pData, int numFramesAvailable) = 0;
	virtual void popData(void *pData, int size) = 0;
	virtual void setAudioFormat(int channels, int samplesRate, int wBitsPerSamples) = 0;
};

class Collocter
{
public:
	Collocter();
	~Collocter();

public:
	// if the GUID is empty ,will select a defaut device
	// if the isInputDevce is false ,we will collect data from render devce, and the value of sampleRate is invalid
	bool Init(bool isInputDevice, int channels, int samplesRate, int wBitsPerSamples, CollocterCallBack *cb, std::string GUID = "");
	void Start();
	void Stop();

private:
	void run();

private:
	CollocterCallBack *pCb;
	IAudioCaptureClient *pCaptureClient_;
	IAudioClient *pAudioClient_;
	__int64 hnsActualDuration_;
	bool bStop_;
	std::thread *pThread_;
};

