// CollectAudioData.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "Collocter.h"
#include <Windows.h>
#include <stdio.h>


class WriteFileCB : public CollocterCallBack
{
public:
	WriteFileCB(const std::string &fileName) : pFile_(NULL){
		fopen_s(&pFile_, fileName.c_str(), "wb");
	}
	~WriteFileCB(){
		if (pFile_) {
			fclose(pFile_);
		}
	}
public:
	void pushData(void *pData, int numFramesAvailable) override {
		if (pFile_) {
			fwrite(pData, numFramesAvailable * channels_ * wBitsPerSamples_ / 8, 1, pFile_);
			fflush(pFile_);
			//printf("adasdf\n");
		}
	}
	void popData(void *pData, int size) override {

	}
	void setAudioFormat(int channels, int samplesRate, int wBitsPerSamples) override {
		channels_ = channels;
		wBitsPerSamples_ = wBitsPerSamples;
		printf("samplesRate\n");

	}

private:
	FILE *pFile_;
	int channels_;
	int wBitsPerSamples_;
};

int _tmain(int argc, _TCHAR* argv[])
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	Collocter collocter;
	Collocter collocter1;
	WriteFileCB writeFile(std::string("G:/abaab.pcm"));
	WriteFileCB writeFile1(std::string("G:/abaab222.pcm"));
	collocter.Init(true, 1, 44100, 16, &writeFile);
	collocter.Start();

	collocter1.Init(false, 2, 48000, 16, &writeFile1);
    collocter1.Start();

	getchar();
	collocter.Stop();
	collocter1.Stop();
	return 0;
}

