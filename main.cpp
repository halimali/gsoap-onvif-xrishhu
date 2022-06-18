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
	if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(proxyDevice.soap, NULL, "admin", DEV_PASSWORD))
	{
		return -1;
	}

	if (SOAP_OK != soap_wsse_add_Timestamp(proxyDevice.soap, "Time", 10)) 
	{
		return -1;
	}

    _tds__GetCapabilities *tds__GetCapabilities = soap_new__tds__GetCapabilities(soap, -1);
	tds__GetCapabilities->Category.push_back(tt__CapabilityCategory__All);

	_tds__GetCapabilitiesResponse *tds__GetCapabilitiesResponse = soap_new__tds__GetCapabilitiesResponse(soap, -1);

	if (SOAP_OK == proxyDevice.GetCapabilities(tds__GetCapabilities, tds__GetCapabilitiesResponse))
    {
        if(tds__GetCapabilitiesResponse->Capabilities->Events != NULL)
        {

            printf("have events capabilities\n");

            std::cout << "XAddr: " << tds__GetCapabilitiesResponse->Capabilities->Events->XAddr.c_str() << std::endl;

            PullPointSubscriptionBindingProxy EventsProxy(
                        tds__GetCapabilitiesResponse->Capabilities->Events->XAddr.c_str());
            EventsProxy.soap_endpoint = tds__GetCapabilitiesResponse->Capabilities->Events->XAddr.c_str();

            if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(EventsProxy.soap, NULL, "admin", DEV_PASSWORD))
            {
                return -1;
            }

            if (SOAP_OK != soap_wsse_add_Timestamp(EventsProxy.soap, "Time", 10)) 
            {
                return -1;
            }

            _tev__GetEventProperties gep;
            _tev__GetEventPropertiesResponse gepr;
            if (EventsProxy.GetEventProperties(&gep, &gepr) == SOAP_OK)
            {
                printf("Event: TopicNamespaceLocation\n");
                for (size_t i = 0; i < gepr.TopicNamespaceLocation.size(); i++)
                {
                    std::cout << gepr.TopicNamespaceLocation.at(i).c_str() << std::endl;
                }
            }
            else
            {
                EventsProxy.soap_stream_fault(std::cerr);
            }
     
            if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(EventsProxy.soap, NULL, "admin", DEV_PASSWORD))
            {
                return -1;
            }

            if (SOAP_OK != soap_wsse_add_Timestamp(EventsProxy.soap, "Time", 10)) 
            {
                return -1;
            }

            _tev__CreatePullPointSubscription cpps;
            _tev__CreatePullPointSubscriptionResponse cppr;
            std::string termtime = "PT60S";
            cpps.InitialTerminationTime = &termtime;

            if (EventsProxy.CreatePullPointSubscription(&cpps, &cppr) == SOAP_OK)
            {
                printf("Event: pull point subscription created\n");
            
                std::cout << "SubscriptionReference.Address: " << cppr.SubscriptionReference.Address << std::endl;

                std::cout << "SubscriptionReference.__size: " << cppr.SubscriptionReference.__size << std::endl;

                std::string sSubscriptionAddress = cppr.SubscriptionReference.Address;
                PullPointSubscriptionBindingProxy   PullPointProxy(sSubscriptionAddress.c_str());

                if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(PullPointProxy.soap, NULL, "admin", DEV_PASSWORD))
                {
                    return -1;
                }

                if (SOAP_OK != soap_wsse_add_Timestamp(PullPointProxy.soap, "Time", 10)) 
                {
                    return -1;
                }

               soap_register_plugin(PullPointProxy.soap, soap_wsa);

                while(1)
                {
                    printf("PULLING message\n");

                    if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(PullPointProxy.soap, NULL, "admin", DEV_PASSWORD))
                    {
                        return -1;
                    }

                    if (SOAP_OK != soap_wsse_add_Timestamp(PullPointProxy.soap, "Time", 10)) 
                    {
                        return -1;
                    }

                   if (SOAP_OK != soap_wsa_request(PullPointProxy.soap,
                                                    NULL,
                                                    sSubscriptionAddress.c_str(),
                                                    "http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/PullMessagesRequest") ||
                       (SOAP_OK != soap_wsa_add_ReplyTo(PullPointProxy.soap,
                                                        "http://www.w3.org/2005/08/addressing/anonymous"))
                       )
                    {
                        PullPointProxy.soap_stream_fault(std::cerr);                   
                    }

                    _tev__PullMessages  PullMessages;
                    //PullMessages.Timeout = "PT10S";
                    PullMessages.Timeout = 60;
                    PullMessages.MessageLimit = 100;
                    _tev__PullMessagesResponse PullMessagesResponse;

                    int nRetCode = SOAP_OK;
                    if ((nRetCode = PullPointProxy.PullMessages(&PullMessages, &PullMessagesResponse)) == SOAP_OK)
                    {
                        for (auto msg : PullMessagesResponse.wsnt__NotificationMessage)
                        {
                            std::cout << "msg:" << msg->Message.__any << std::endl;
                        }
                    }
                    else
                    {
                        PullPointProxy.soap_stream_fault(std::cerr);
                    }

                    sleep(5);
                }
            }                             
            else
            {
                EventsProxy.soap_stream_fault(std::cerr);
            }
        }
        else
        {
            printf("events capabilities null!\n");
        }

    }
	else
	{
		PrintErr(proxyDevice.soap);
	}

	soap_destroy(soap); 
	soap_end(soap); 

	return 0;
}

