/*------------------------------------------------------------  
����: Buf         (double������)
      Length      (Buf����,Ҳ����������)     
	  SampleRate  (������)
      Vad_value   (���ڱ���VAD�˵�����,��С Vad_num*2)
	  Vad_num     (���ڱ���VAD�˵���) 
���: ����flag = 1,�˵���ɹ�; ����flag = -1,�˵���ʧ��
---------------------------------------------------------------*/
#ifdef  VAD_EXPORTS
#define VAD_API __declspec(dllexport)
#else
#define VAD_API __declspec(dllimport)
#endif

extern "C" VAD_API double* VADB(short* buf,double SampleRate,int SamplePoints,int& seg_num);





