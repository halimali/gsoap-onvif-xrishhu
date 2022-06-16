#include <iostream>
#include "stdio.h"
#include "wsdd.nsmap"
#include "plugin/wsseapi.h"
#include "plugin/wsaapi.h"
#include  <openssl/rsa.h>
#include  "ErrorLog.h"
 
#include "include/soapDeviceBindingProxy.h"
#include "include/soapMediaBindingProxy.h"
#include "include/soapPTZBindingProxy.h"

#include "include/soapPullPointSubscriptionBindingProxy.h"
#include "include/soapRemoteDiscoveryBindingProxy.h" 

using namespace std;

#define DEV_PASSWORD "ste12345"
#define MAX_HOSTNAME_LEN 128
#define MAX_LOGMSG_LEN 256 


void PrintErr(struct soap* _psoap)
{
	fflush(stdout);
	processEventLog(__FILE__, __LINE__, stdout, "error:%d faultstring:%s faultcode:%s faultsubcode:%s faultdetail:%s", _psoap->error, 
	*soap_faultstring(_psoap), *soap_faultcode(_psoap),*soap_faultsubcode(_psoap), *soap_faultdetail(_psoap));
}

int main(int argc, char* argv[])
{
	struct soap *soap = soap_new();

	bool blSupportPTZ = false;
	char szHostName[MAX_HOSTNAME_LEN] = { 0 };
	char sLogMsg[MAX_LOGMSG_LEN] = { 0 };

	DeviceBindingProxy proxyDevice;
	RemoteDiscoveryBindingProxy proxyDiscovery; 
	MediaBindingProxy proxyMedia;
	PTZBindingProxy proxyPTZ;
	PullPointSubscriptionBindingProxy proxyEvent(soap);

	if (argc > 1)
	{
		strcat(szHostName, "http://");
		strcat(szHostName, argv[1]);
		strcat(szHostName, "/onvif/device_service");

		proxyDevice.soap_endpoint = szHostName;
		proxyEvent.soap_endpoint = szHostName;
	}
	else
	{
		processEventLog(__FILE__, __LINE__, stdout, "wrong args,usage: ./a.out 172.18.4.100 ");
		return -1;
	}

	soap_register_plugin(proxyDevice.soap, soap_wsse);
	soap_register_plugin(proxyDiscovery.soap, soap_wsse);
	soap_register_plugin(proxyMedia.soap, soap_wsse);
	soap_register_plugin(proxyPTZ.soap, soap_wsse);
	soap_register_plugin(proxyEvent.soap, soap_wsse);

	soap_register_plugin(proxyEvent.soap, soap_wsa);


	if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(proxyEvent.soap, NULL, "admin", DEV_PASSWORD))
	{
		return -1;
	}

	if (SOAP_OK != soap_wsse_add_Timestamp(proxyEvent.soap, "Time", 10)) 
	{
		return -1;
	}

	_tev__CreatePullPointSubscription request;
	_tev__CreatePullPointSubscriptionResponse response;
	auto ret = proxyEvent.CreatePullPointSubscription(&request, &response);
	if (ret != SOAP_OK)
	{
		soap_stream_fault(soap, std::cerr);
	}
	else
	{
		auto address = response.SubscriptionReference.Address;
		std::cout << address << std::endl;
		std::cout << "Subscription metadata: " << response.SubscriptionReference.Metadata << std::endl;
		std::cout << "Termination time " << response.wsnt__TerminationTime << std::endl;
		std::cout << "Current time " << response.wsnt__CurrentTime << std::endl;

		std::string uuid = std::string(soap_rand_uuid(soap, "urn:uuid:"));
		struct SOAP_ENV__Header header;
		header.wsa5__MessageID = (char *)uuid.c_str();
		header.wsa5__To = response.SubscriptionReference.Address;
		soap->header = &header;

		_tev__PullMessages tev__PullMessages;
		tev__PullMessages.Timeout = 600;
		//tev__PullMessages.Timeout = "PT600S";
		tev__PullMessages.MessageLimit = 100;
		_tev__PullMessagesResponse tev__PullMessagesResponse;

		auto ret = proxyEvent.PullMessages(&tev__PullMessages, &tev__PullMessagesResponse);
		for (auto msg : tev__PullMessagesResponse.wsnt__NotificationMessage)
		{
			std::cout << "\tMessage is :" << msg->Topic->__mixed << std::endl;
		}
	}

	proxyEvent.destroy();

	soap_destroy(soap); 
	soap_end(soap); 

/*
	_tds__GetDeviceInformation *tds__GetDeviceInformation = soap_new__tds__GetDeviceInformation(soap, -1);
	_tds__GetDeviceInformationResponse *tds__GetDeviceInformationResponse = soap_new__tds__GetDeviceInformationResponse(soap, -1);

	if (SOAP_OK == proxyDevice.GetDeviceInformation(tds__GetDeviceInformation, tds__GetDeviceInformationResponse))
	{
		processEventLog(__FILE__, __LINE__, stdout, "-------------------DeviceInformation-------------------");
		processEventLog(__FILE__, __LINE__, stdout, "Manufacturer:%sModel:%s\r\nFirmwareVersion:%s\r\nSerialNumber:%s\r\nHardwareId:%s", tds__GetDeviceInformationResponse->Manufacturer.c_str(),
			tds__GetDeviceInformationResponse->Model.c_str(), tds__GetDeviceInformationResponse->FirmwareVersion.c_str(),
			tds__GetDeviceInformationResponse->SerialNumber.c_str(), tds__GetDeviceInformationResponse->HardwareId.c_str());
	}
	else
	{
		PrintErr(proxyDevice.soap);
	}
*/
	
	return 0;
}

