#ifndef AW_SER_DEV_H
#define AW_SER_DEV_H

typedef void* AWSERDEVHANDLE;
typedef AWSERDEVHANDLE* PAWSERDEVHANDLE;

/*
���ܣ�
	��ʼ���������������ز��ʵ����
������
    szError[out]---��Ŵ�����Ϣ
    iLen[in]---szError��С��iLen��С��MAX_PATH
���أ�
    ��ʼ���ɹ������ز����������򣬷���NULL
*/
AWSERDEVHANDLE AwsInitSerDevLibrary(char* szError, int iLen);


/*
���ܣ�
	�ͷ����������������pHandle��NULL��
������
	pHandle[in_out]---������
���أ�
    ��
*/
void AwsReleaseSerDevLibrary(PAWSERDEVHANDLE pHandle);


/*
���ܣ�
    ֪ͨPC��ʼ���ԡ�
������
    szId[in]---msgid
    testNames[in]---���в����������
    iTestCount[in]---���������Ƶ�����
���أ�
    ���ز����������
*/
int AwsTestBegin(char* szId, char** testNames, int iTestCount);


/*
���ܣ�
    ֪ͨPC������ȫ��������
������
    szAuthor[in]---������Ա
    iPassed[in]---�����Ƿ�ͨ����1��ʾͨ����0��ʾ��ͨ��
    iErrorCode[in]---�����
    szError[in]---�����������ò�����ΪNULL
���أ�
    ���ز����������
*/
int AwsTestFinish(char* szAuthor, int iPassed, int iErrorCode, char* szError);


/*
���ܣ�
    ֪ͨ�豸�˲����Ƿ��������ÿ�����в�����ǰ��������ñ��������ж��Ƿ�������ԡ��������������0����Ӧ�ý������в��Թ�����
������
    ��
���أ�
    1---��������
    0---��������
*/
int AwsTestContinue();


/*
���ܣ�
    ֪ͨPC��ʼ����ĳ�������
������
    iTestId[in]---������ID
    szTip[in]---��ʾ��Ϣ���ò�����ΪNULL
���أ�
    ������ID
*/
int AwsTestBeginItem(int iTestId, char* szTip);


/*
���ܣ�
    ֪ͨPCĳ���������ѽ������ԡ�
������
    iTestId[in]---������ID
    iPassed[in]---�����Ƿ�ͨ����1��ʾͨ����0��ʾ��ͨ��
    iErrorCode[in]---�����
    szError[in]---�����������ò�����ΪNULL
���أ�
    ������ID
*/
int AwsTestFinishItem(int iTestId, int iPassed, int iErrorCode, char* szError);


/*
���ܣ�
    ��ȡPC�Ĳ�����Ӧ��
������
    iTestId[in]---������ID
    iTimeOut[in]---�ȴ�PC��Ӧ��ʱ�䣨���룩�����Ϊ-1��һֱ�ȴ�
    strTip[in]---��ʾ��Ϣ���ò�����ΪNULL
    piResponse[in]---PC��Ӧ�Ľ����0��ʾ��ʱδ��Ӧ��1��ʾPASSED��2��ʾFAILED
���أ�
    0---�������óɹ�
    ����---��������ʧ��
*/
int AwsTestResponse(int iTestId, int iTimeOut, const char* szTip, int* piResponse, char* szComment);
#endif
