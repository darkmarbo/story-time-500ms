#include"VADB.h"
#include <iostream>
#include <string.h>
#include <vector>
#include <windows.h>               //ʹ�ú�����ĳЩ�������



using namespace std;

const int MAX_LEN = 1000;
const double TIME_SIL = 0.5;


//typedef void (*DLLFunc)(int,int);  //ȷ�����ú������β�
typedef int (*DLLFunc_a)(const char *, const char *, double &,double &);
typedef double* (*dll_VADB)(short* ,double ,int ,int& );


//			fseek(fp,(long)(head.t1-16)-4,SEEK_CUR);
//		fread(&head.sumbytes, 1, sizeof(long), fp);
typedef struct _wavhead
{
	char            riff[4];            //4		��RIFF��;   RIFF��־
	unsigned long   filelong;           //4		0x00 01 06 0A��ע�����ݴ洢˳��;   �ļ�����
	char            wav[8];             //8		"WAVEfmt "
	unsigned long   t1;					//4		0x12;   sizeof(PCMWAVEFORMAT)         
	short           tag;				//2		1��WAVE_FORMAT_PCM��;  ��ʽ���1��ʾΪPCM��ʽ����������
	short           channels;			//2		2;  ͨ������������Ϊ1��˫����Ϊ2
	unsigned long   samplerate;			//4		44100;  ����Ƶ�ʣ�ÿ����������    
	unsigned long   typesps;			//4		0x10B10000;   ÿ������������ֵΪͨ������ÿ������λ����ÿ����������λ����8������������ô�ֵ���Թ��ƻ������Ĵ�С��
	unsigned short  psbytes;            //2		���ݿ�ĵ����������ֽ���ģ�����ֵΪͨ������ÿ����������λֵ��8�����������Ҫһ�δ�������ֵ��С���ֽ����ݣ��Ա㽫��ֵ���ڻ������ĵ�����
	unsigned short  psbits;             //2		ÿ����������λ������ʾÿ�������и�������������λ��������ж����������ÿ���������ԣ�������С��һ����
	char            data[4];            //4		��data��;   ���ݱ�Ƿ�
	unsigned long   sumbytes;           //4		0x00 01 05 D8;   �������ݴ�С
}WAVEHEAD;

// �洢 lab ÿ�еĽṹ�� 
typedef struct _lab
{
	std::string name; // ��������  track1
	double st; // ��ʼʱ��  
	double end; // ����ʱ�� 
	std::string cont; // �洢����
}LAB;

int ReadFileLength(const char *wfile,int* sampleRate);
int ReadFile(const char *wfile, short* allbuf, int bias, int halfWindow);
/*  p_lab ���ٵ���Ҫ�㹻�� len_lab */
int read_lab(const char *file_lab, LAB *p_lab, int len_lab );
int read_wav_head(const char *wfile, WAVEHEAD &head);
int write_wav(const char *file, WAVEHEAD *wav_head, short *buff, int len_buff);
int adjust_pos(double &pos, int flag, double st_end);
void split(std::string& s, std::string& delim, std::vector< std::string >* ret);
int check_buff(short *buff, int len, int flag);


int main(int argc, char* argv[])
{
	if(argc < 4)
	{
		printf("usage: %s wav lab out.txt",argv[0]);
		printf("%s #����Ŀ¼����# #У�Թ����ı���ʱ�����Ϣ#  #�޸Ĺ��ı�ʱ���# ",argv[0]);
		return 0;
	}

	// ����vadģ��
	HINSTANCE hInstLibrary = LoadLibrary(L"VADB.dll"); //����.dll
	if (hInstLibrary == NULL)
	{
		FreeLibrary(hInstLibrary);
		printf("hInstLibrary == NULL\n");
		return 0;
	}


	dll_VADB dll_VADB_fun;
	dll_VADB_fun = (dll_VADB)GetProcAddress(hInstLibrary, "VADB");
	if (dll_VADB_fun == NULL)
	{
		FreeLibrary(hInstLibrary);
		printf("dllFunc_a == NULL\n");
		return 0;
	}
	
	// �������� 
	int ret = 0;
	
	double a=1.0;
	double b=0.0;
	int smp_rate_int = 0;  // ������
	double smp_rate = 0.0;
	int smp_count = 0;  // ��ȡ�����ļ��ܳ��� ������֡��
	double time_wav = 0.0;  // ������Ӧ��ʱ�䳤�� ��λ��

	int num = 0;
	int num_all = 0;
	int len_lab = 0;

	LAB *p_lab = new LAB[MAX_LEN];
	WAVEHEAD wav_head;
	
	
	// �洢vad��Ľ��  [0.01,1.23],[1.50,2.45]
	double *seg_small = NULL; 
	
	char *dir_wav = argv[1];	// ��story��ȫ��wavĿ¼ 
	char *file_lab = argv[2];	// �˹��������� name st end txt �ļ� 
	char *file_outlog = argv[3];// ������ 

	printf("%s\t%s\n",dir_wav, file_lab);
	FILE *fp_out = fopen(file_outlog,"w");
	FILE *fp_log = fopen("vad.log","w");

	/*
	std::string str_lab = file_lab;
	int pos_lab = str_lab.rfind("\\");
	str_lab = str_lab.substr(pos_lab + 1);
	pos_lab = str_lab.rfind(".lab");
	str_lab = str_lab.substr(0, pos_lab);
	*/

	//  �Ӵ�txt�ı��ж�ȡ  ��name st end txt�� LAB 
	len_lab = read_lab(file_lab, p_lab, MAX_LEN);
	if (len_lab < 1)
	{
		printf("read lab file err!");
		return 0;
	}



	//  ��txt��ÿһ�� ����vad ���� 
	for (int ii = 0; ii < len_lab; ii++)
	{
		std::string track_name = (p_lab + ii)->name;
		double st = (p_lab + ii)->st;
		double end = (p_lab + ii)->end;
		std::string cont = (p_lab + ii)->cont;
		// ԭʼ�� st��end 
		fprintf(fp_out, "%s\t%.4f\t%.4f\t%s\n",
			track_name.c_str(), st, end, cont.c_str());

		std::string  wav_name = track_name;
		wav_name += std::string(".wav");
		std::string wav_path = std::string(dir_wav);
		wav_path += "\\";
		wav_path += wav_name;

		// ��ȡ�������� 
		// ԭʼ�� wav_head  ����ÿ���ļ� ��Ҫ�޸� 
		//ret = read_wav_head(wav_path, wav_head); 

		//// ��ȡ�����ʺͲ��������� 
		smp_count = ReadFileLength(wav_path.c_str(), &smp_rate_int); 
		time_wav = double(smp_count)/double(smp_rate_int);
		smp_rate = double(smp_rate_int);
		fprintf(fp_log,"wav=%s\tsmp_rate=%d\tsmp_count:%d\n",
			wav_path.c_str(), smp_rate_int, smp_count);

		//// �洢����������buff���� 
		short *buff = new short[smp_count];  
		ret = ReadFile(wav_path.c_str(), buff, 0, smp_count);
		if (ret < 0)
		{
			printf("ERROR:readFile %s err!\n",wav_path);
			return 0;
		}
		
		////// vad��� �õ�ÿ��С�ε� st��end  
		int count_left = int(st*smp_rate);  // ����Ӧ������
		int count_right = int(end*smp_rate);  // �Ҳ��Ӧ������
		 
		
		//// vad���� buff, smp_rate, smp_count, num
		seg_small = dll_VADB_fun(buff+count_left, smp_rate, count_right-count_left, num);
		
		//// ��num��segment �ϲ���һ��  Ҳ����ֻѡȡ st1 �� end_num   	
		double st_new = st + seg_small[0];
		double end_new = st + seg_small[2*num-1];
		// vad����� �� st end  ��� 
		fprintf(fp_out, "vad:%s\t%.4f\t%.4f\t%s\n", track_name.c_str(), st_new, end_new, cont.c_str());
		fflush(fp_out);
		
		// 500ms��Ӧ�Ĳ����� ����
		double time_left;
		double time_right;
		// ������buff �ڴ�buff��λ�� ��Ӧ��ȫ��ʱ��  
		time_left = st_new > TIME_SIL ? st_new - TIME_SIL : 0.01;
		time_right = end_new + TIME_SIL < time_wav ? end_new + TIME_SIL : time_wav-0.01;
		// ������buff�ڴ�buff�е� ��ʼλ�� 
		short *buff_left = buff + int((time_left)*smp_rate);
		short *buff_right = buff + int((end_new)*smp_rate);
		// �����buff�ĳ��� 
		int cnt_len_left = int((st_new - time_left)*smp_rate);
		int cnt_len_right = int((time_right-end_new)*smp_rate);
		
		// ����� �õ������ƶ��Ĳ�������   
		int find_left = check_buff(buff_left, cnt_len_left, 1);
		int find_right = check_buff(buff_right, cnt_len_right, 0);

		st_new -= double(find_left) / double(smp_rate);
		end_new += double(find_right) / double(smp_rate);
		// �ƶ��� �� st end  ��� 
		fprintf(fp_out, "pos:%s\t%.4f\t%.4f\t%s\n", track_name.c_str(), st_new, end_new, cont.c_str());
		fflush(fp_out);

		//// �˵���� 
		//ret = adjust_pos(st_new, 1, st);
		//if (ret != 0)
		//{
		//	printf("ERROR:vad processed result err!");
		//	continue;
		//}
		//ret = adjust_pos(end_new, 0, end);
		//if (ret != 0)
		//{
		//	printf("ERROR:vad processed result err!");
		//	continue;
		//}
		//
		//
		//for (int ii = 0; ii < num; ii++)
		//{
		//	fprintf(fp_log, "%d\t%.4f\t%.4f\n", ii,
		//		seg_small[2 * ii], seg_small[2 * ii + 1]);
		//	fflush(fp_log);
		//}



		
		//// ͬʱ���ö� ���� ���
		//std::string wav_path_out = str_lab;
		//char str_d[20] = {0};
		//sprintf_s(str_d, "_%.3f", st_new);
		//wav_path_out += std::string(str_d);
		//sprintf_s(str_d, "_%.3f.wav", end_new);
		//wav_path_out += std::string(str_d);
		//
		//FILE *fp_temp = NULL;
		//if ((fp_temp = fopen(wav_path_out.c_str(), "w")) == NULL)
		//	return -1;
		//
		//
		//int count_out = (end_new - st_new)*smp_rate;
		//wav_head.sumbytes = count_out*sizeof(short);
		//
		//// ���ĸ�point��ʼд   д����   д���ٸ� 
		//fwrite(&wav_head, sizeof(WAVEHEAD), 1, fp_temp);
		//fwrite(buff + int(st_new*smp_rate), sizeof(short), count_out, fp_temp);
		//
		//if (fp_temp){ fclose(fp_temp); fp_temp = NULL; }
		
	}


						

			
	// �ͷ� �� �ر� 
	if (hInstLibrary){ FreeLibrary(hInstLibrary);  hInstLibrary= NULL;}
	//if (buff){ delete buff; buff = NULL; }
	if (fp_log){ fclose(fp_log); fp_log = NULL; }
	if (fp_out){ fclose(fp_out); fp_out = NULL; }
	if (p_lab){ delete []p_lab; p_lab = NULL; }

	return 0;

	
}

/*
	���buff �����������Ƿ��Ǿ���
	
	flag=1�����   flag=0���Ҳ�
	���� ��������Ĳ�������  
*/
int check_buff(short *buff, int len, int flag)
{
	if (!buff){ return -1; }
	int ret = 0;
	int len_wind = 10;  // ���� 
	int len_ok = 4; // ����10��������  ��5�� ��ֵ���� 400 
	int max_value = 500;
	

	if (flag == 1)  // ��������  ���Ҳ࿪ʼ 
	{
		int ii = len - len_wind;
		for (; ii > -1; ii--)
		{
			int count_err = 0;

			for (int jj = 0; jj < len_wind; jj++)
			if (buff[ii + jj]>max_value || buff[ii + jj] < 0 - max_value)
				count_err++;

			if (count_err>len_ok)
				return (len-len_wind)-ii;
		}
		return len - len_wind;
	}
		
	if (flag == 0)  // �Ҳ�����  ����࿪ʼ 
	{
		int ii = 0;
		for (; ii < len - len_wind; ii++)
		{
			int count_err = 0;
			for (int jj = 0; jj < len_wind; jj++)
			if (buff[ii + jj]>max_value || buff[ii + jj] < 0 - max_value)
				count_err++;

			if (count_err>len_ok)return ii;
		}
		return len - len_wind;
	}
	
	
}

/*
	flag=1��ʾst �����ʾend,���ط�0ʧ��
*/
int adjust_pos(double &pos,int flag,double st_end)
{
	// ������ 200ms ��Ȼ����ԭ���Ŀ�� 
	double POS_1 = 0.40;
	double POS_2 = 0.30;
	double POS_3 = 0.20;
	double POS_4 = 0.10;

	if (st_end<0)
	{
		printf("ERROR:input st_end=%.4f err!\n",st_end);
		return -1;
	}

	if (flag == 1) // st
	{
		if (pos - POS_1>st_end )
		{
			pos -= POS_1;
		}
		else if (pos - POS_2>st_end )
		{
			pos -= POS_2;
		}
		else if (pos - POS_3>st_end)
		{
			pos -= POS_3;
		}
		else if (pos - POS_4>st_end)
		{
			pos -= POS_4;
		}
		else
		{
			pos = st_end + 0.01;
		}
	}
	else // end
	{
		if (pos>st_end)
		{
			printf("ERROR:input st_end=%.4f\tpos=%.4f err!\n",st_end,pos);
			return -2;
		}
		if (pos + POS_1< st_end)
		{
			pos += POS_1;
		}
		else if (pos + POS_2<st_end)
		{
			pos += POS_2;
		}
		else if (pos + POS_3<st_end)
		{
			pos += POS_3;
		}
		else if (pos + POS_4<st_end)
		{
			pos += POS_4;
		}
		else
		{
			pos = st_end - 0.01;
		}
	}


	return 0;
}

//ע�⣺���ַ���Ϊ��ʱ��Ҳ�᷵��һ�����ַ���  
void split(std::string& s, std::string& delim, std::vector< std::string >* ret)
{
	size_t last = 0;
	size_t index = s.find_first_of(delim, last);
	while (index != std::string::npos)
	{
		ret->push_back(s.substr(last, index - last));
		last = index + 1;
		index = s.find_first_of(delim, last);
	}
	if (index - last>0)
	{
		ret->push_back(s.substr(last, index - last));
	}
}

int read_lab(const char *file_lab, LAB *p_lab, int len_lab )
{
	// ��ȡ��lab����
	int len_read = 0;
	char chr_line[MAX_LEN] = {0};
	int pos_st = 0;

	FILE *fp_lab = fopen(file_lab, "r");
	if(!fp_lab){
		printf("��ȡlab=%s�ļ�ʧ��",file_lab);
		return len_read;
	}

	while(fgets(chr_line, MAX_LEN, fp_lab))
	{
		//printf("line=%s",chr_line);
		int ii = 0;
		// track_01	2.122540	3.842540	by sarah courtauld
		std::string str_line = chr_line;

		// ɾ����ĩβ�� \n
		if (str_line.substr(str_line.length() - 1).compare("\n") == 0)
		{
			str_line = str_line.substr(0, str_line.length() - 1);
		}

		// �зֳ� 4 ��
		std::vector <std::string> vec_split;
		std::string delim = "\t";
		split(str_line, delim, &vec_split);
		if (vec_split.size() < 4)
		{
			printf("ERROR:lab line format err:%s", chr_line);
			return 0;
		}
			
		std::string track_name = vec_split[0]; // 02_Track_02
		std::string str_st = vec_split[1];
		std::string str_end = vec_split[2];
		std::string content = vec_split[3];

		if (content.compare("#") == 0 || content.compare("#\n") == 0 ||
			content.length()<1)
		{
			printf("ERROR��content=%s\t\n", content.c_str());
			continue;
		}

		// ��ӵ� p_lab �� 
		p_lab->name = track_name;
		p_lab->st = atof(str_st.c_str());
		p_lab->end = atof(str_end.c_str());
		p_lab->cont = content;

		p_lab += 1;
		len_read += 1;

		if (len_read > len_lab - 1)
		{ 
			printf("input len_lab is too small !\n"); 
			return 0; 
		}

	}
	
	return len_read;

}

// ��short buff д�� wav�� 
/*
	buff ---> ***.wav
	�� buff��ʼ дlen_buff��short ���ļ�wfile�� 
*/
int write_wav(const char *file, WAVEHEAD *wav_head, short *buff, int len_buff)
{
	int ret = 0;
	FILE *fp = NULL;
	if ((fp = fopen(file, "w")) == NULL)
		return -1;

	return ret;
}

/*
	��ȡ����wav��ͷ  WAVEHEAD 
*/
int read_wav_head(const char *wfile, WAVEHEAD &head)
{
	bool oflag = false;
	FILE *fp = NULL;
	int SAMFREQ = -1;
	int sample_count = 0, channel_num = 0, readflag = 0;
	int numSample = 0;//�����ݳ���
	try
	{
		//�ж������ļ�
		if (strstr(wfile, ".wav")) {
			fp = fopen(wfile, "rb");
			if (fp == NULL) {
				return -2;
			}
			oflag = true;
			fseek(fp, 0, SEEK_END);
			sample_count = ftell(fp) - sizeof(WAVEHEAD);
			fseek(fp, 0, SEEK_SET);
			fread(&head, 1, sizeof(WAVEHEAD), fp);
			//data
			if (head.data[0] != 'd'&&head.data[1] != 'a'&&head.data[2] != 't'&&head.data[3] != 'a')
			{
				fclose(fp);
				return -3;
			}
			//RIFF
			if (head.riff[0] != 'R'&&head.riff[1] != 'I'&&head.riff[2] != 'F'&&head.riff[3] != 'F')
			{
				fclose(fp);
				return -3;
			}
			//"WAVEfmt "
			if (head.wav[0] != 'W'&&head.wav[1] != 'A'&&head.wav[2] != 'V'&&head.wav[3] != 'E'&&head.wav[4] != 'f'&&head.wav[5] != 'm'&&head.wav[6] != 't'&&head.wav[7] != ' ')
			{
				fclose(fp);
				return -3;
			}
			//��λ����
			fseek(fp, (long)(head.t1 - 16) - 4, SEEK_CUR);
			fread(&head.sumbytes, 1, sizeof(long), fp);
			//�õ��ֽ���
			sample_count = head.sumbytes;
			if (head.samplerate>48000 || head.samplerate<0)
			{
				fclose(fp);
				exit(-1);
			}
			SAMFREQ = head.samplerate;
			channel_num = head.channels;
		}
		//�õ���������n��ͨ���������ͣ���Ϊ16bit��
		sample_count /= sizeof(short);
		if (sample_count % channel_num != 0) {
			fclose(fp);
			return -4;
		}

		fclose(fp);
		oflag = false;
	}
	catch (...)
	{
		if (oflag)
			fclose(fp);

		return -6;
	}

	return 0;
}

// ��bias����ʼ��ȡ halfWindow ��short�� �������������-1��
int ReadFile(const char *wfile, short* allbuf, int bias, int halfWindow)
{
	bool oflag=false;
	FILE *fp=NULL;
	WAVEHEAD head;
	int SAMFREQ=-1;
	int sample_count=0,channel_num=0,readflag=0;
	int numSample = 0;//�����ݳ���
	try
	{
		//�ж������ļ�
		if (strstr(wfile, ".wav")) {
			fp=fopen(wfile, "rb");
			if (fp == NULL) {
				return -2;
			}
			oflag=true;
			fseek(fp,0,SEEK_END);
			sample_count = ftell(fp) - sizeof(WAVEHEAD);
			fseek(fp,0,SEEK_SET);
			fread(&head, 1, sizeof(WAVEHEAD), fp);
			//data
			if(head.data[0]!='d'&&head.data[1]!='a'&&head.data[2]!='t'&&head.data[3]!='a')
			{
				fclose(fp);
				return -3;
			}
			//RIFF
			if(head.riff[0]!='R'&&head.riff[1]!='I'&&head.riff[2]!='F'&&head.riff[3]!='F')
			{
				fclose(fp);
				return -3;
			}
			//"WAVEfmt "
			if(head.wav[0]!='W'&&head.wav[1]!='A'&&head.wav[2]!='V'&&head.wav[3]!='E'&&head.wav[4]!='f'&&head.wav[5]!='m'&&head.wav[6]!='t'&&head.wav[7]!=' ')
			{
				fclose(fp);
				return -3;
			}
			//��λ����
			fseek(fp,(long)(head.t1-16)-4,SEEK_CUR);
			fread(&head.sumbytes,1,sizeof(long),fp);
			//�õ��ֽ���
			sample_count=head.sumbytes;
			if(head.samplerate>48000||head.samplerate<0)
			{
				fclose(fp);
				exit(-1);
			}
			SAMFREQ = head.samplerate;
			channel_num = head.channels;
		}
		//�õ���������n��ͨ���������ͣ���Ϊ16bit��
		sample_count /= sizeof(short);
		if (sample_count % channel_num != 0) {
			fclose(fp);
			return -4;
		}
		// ����ռ��ȡ����
		// ��bias�Ŀ�ʼ��ȡ halfWindow ��short�� �������������-1��
		printf("bias=%d\tsample_count=%d\thalfWindow=%d\n",bias, sample_count,halfWindow);
		if (bias + halfWindow <= sample_count)
		{
			numSample = halfWindow;
		}
		else
		{
			return -5;
		}
		//allbuf = (short*)malloc(numSample * sizeof(short));
		fseek(fp, bias*sizeof(short), SEEK_CUR);
		fread(allbuf, sizeof(short), numSample,fp);
		
		fclose(fp);
		oflag=false;
	}
	catch(...)
	{
		if(oflag)
			fclose(fp);

		if(allbuf)free(allbuf);
		allbuf=NULL;
		return -6;

	}
	return 0;
}


int ReadFileLength(const char *wfile,int* sampleRate)
{
	bool oflag=false;
	FILE *fp=NULL;
	WAVEHEAD head;
	int SAMFREQ=-1;
	int sample_count=0,channel_num=0,readflag=0;
	int numSample = 0;//�����ݳ���
	try
	{
		//�ж������ļ�
		if (strstr(wfile, ".wav")) {
			fp=fopen(wfile, "rb");
			if (fp == NULL) {
				printf("read %s err!\n", wfile);
				return -1;
			}
			//printf("open file ok!\n");

			oflag=true;
			fseek(fp,0,SEEK_END);
			sample_count = ftell(fp) - sizeof(WAVEHEAD);
			fseek(fp,0,SEEK_SET);
			fread(&head, 1, sizeof(WAVEHEAD), fp);
			//data
			if(head.data[0]!='d'&&head.data[1]!='a'&&head.data[2]!='t'&&head.data[3]!='a')
			{
				fclose(fp);
				printf("read data err!\n");
				return -1;
			}
			//RIFF
			if(head.riff[0]!='R'&&head.riff[1]!='I'&&head.riff[2]!='F'&&head.riff[3]!='F')
			{
				fclose(fp);
				printf("read RIFF err!\n");
				return -1;
			}
			//"WAVEfmt "
			if(head.wav[0]!='W'&&head.wav[1]!='A'&&head.wav[2]!='V'&&head.wav[3]!='E'&&head.wav[4]!='f'&&head.wav[5]!='m'&&head.wav[6]!='t'&&head.wav[7]!=' ')
			{
				fclose(fp);
				printf("read WAVEfmt err!\n");
				return -1;
			}
			//��λ����
			fseek(fp,(long)(head.t1-16)-4,SEEK_CUR);
			fread(&head.sumbytes,1,sizeof(long),fp);
			//�õ��ֽ���
			sample_count=head.sumbytes;
			if(head.samplerate>48000||head.samplerate<0)
			{
				fclose(fp);
				exit(-1);
			}
			SAMFREQ = head.samplerate;
			channel_num = head.channels;

			*sampleRate = SAMFREQ;
		}
		//�õ���������n��ͨ���������ͣ���Ϊ16bit��
		sample_count /= sizeof(short);
		if (sample_count % channel_num != 0) {
			fclose(fp);
			printf("read channel err!\n");
			return -2;
		}
		/*//����ռ��ȡ����
		if (bias+MAX<sample_count)
		{
			numSample = MAX;
		}
		else
		{
			numSample = sample_count-bias;
		}
		allbuf = (short*)malloc(numSample * sizeof(short));
		fread(allbuf, sizeof(short), numSample,fp+bias);
		fclose(fp);
		oflag=false;*/

		fclose(fp);
		return sample_count;
	}
	catch(...)
	{
		if(oflag)
			fclose(fp);

		/*if(allbuf)free(allbuf);
		allbuf=NULL;*/
		return -1;

	}

	fclose(fp);
	return 0;
}