#include "chip.h"
#include "stdbool.h"


//-----------------------------------------------------------------------------
// 1msタイマー割り込み
//-----------------------------------------------------------------------------
void MRT_IRQHandler(void)
{
	uint32_t		intf = 0;


	// 割り込みフラグの取得とクリアを行う
	intf = Chip_MRT_GetIntPending();
	Chip_MRT_ClearIntPending(intf);
}


//-----------------------------------------------------------------------------
// メイン処理
//-----------------------------------------------------------------------------
int main(void)
{

	//▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼
    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();

	// 使用するピンの設定
    Chip_GPIO_Init(LPC_GPIO_PORT);
    Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 0 , 14);
    Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, 0, 0);

    // 1msタイマー開始
    Chip_MRT_Init();
    Chip_MRT_SetMode(LPC_MRT_CH0, MRT_MODE_REPEAT);
    Chip_MRT_SetInterval(LPC_MRT_CH0, (24000-1));			// 1ms
    Chip_MRT_SetEnabled(LPC_MRT_CH0);
    NVIC_EnableIRQ(MRT_IRQn);
    //▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲

    Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , 14);



    while(1)
    {
    	if (Chip_GPIO_GetPinState(LPC_GPIO_PORT, 0, 0) == true)
    	{
    		Chip_GPIO_SetPinToggle(LPC_GPIO_PORT, 0 , 14);
    	}

    	__WFI();
    }




    return 0;
}
