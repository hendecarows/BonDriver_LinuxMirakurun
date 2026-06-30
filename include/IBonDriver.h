// IBonDriver.h: IBonDriver クラスのインターフェイス
//
//////////////////////////////////////////////////////////////////////

#if !defined(_IBONDRIVER_H_)
#define _IBONDRIVER_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <stdint.h>

// 凡ドライバインタフェース
class IBonDriver
{
public:
	virtual bool OpenTuner(void) = 0;
	virtual void CloseTuner(void) = 0;

	virtual bool SetChannel(const uint8_t bCh) = 0;
	virtual float GetSignalLevel(void) = 0;

	virtual uint32_t WaitTsStream(const uint32_t dwTimeOut = 0) = 0;
	virtual uint32_t GetReadyCount(void) = 0;

	virtual bool GetTsStream(uint8_t *pDst, uint32_t *pdwSize, uint32_t *pdwRemain) = 0;
	virtual bool GetTsStream(uint8_t **ppDst, uint32_t *pdwSize, uint32_t *pdwRemain) = 0;

	virtual void PurgeTsStream(void) = 0;

	virtual void Release(void) = 0;
};


// インスタンス生成メソッド
//extern "C" __declspec(dllimport) IBonDriver* CreateBonDriver();

#endif // !defined(_IBONDRIVER_H_)
