// This sample is to demonstrate azure IoT cloud communicator which receives data from client via socket communicatation 
// sends the data to the IOT hub.

#include <stdio.h> 
#include <stdlib.h> 

#ifdef __linux__
#include <netinet/in.h>
#include <sys/socket.h> 
#include <sys/types.h> 
#include <arpa/inet.h> 

#elif _WIN32
#include<winsock2.h>

#endif

#include<string.h>

#include "iothub.h"
#include "iothub_device_client.h"
#include "iothub_client_options.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/shared_util_options.h"

#include<parson.h>

#define SAMPLE_MQTT
#define SAMPLE_MQTT_OVER_WEBSOCKETS
#define SAMPLE_AMQP
#define SAMPLE_AMQP_OVER_WEBSOCKETS

#ifdef SAMPLE_MQTT
    #include "iothubtransportmqtt.h"
#endif // SAMPLE_MQTT
#ifdef SAMPLE_MQTT_OVER_WEBSOCKETS
    #include "iothubtransportmqtt_websockets.h"
#endif // SAMPLE_MQTT_OVER_WEBSOCKETS
#ifdef SAMPLE_AMQP
    #include "iothubtransportamqp.h"
#endif // SAMPLE_AMQP
#ifdef SAMPLE_AMQP_OVER_WEBSOCKETS
    #include "iothubtransportamqp_websockets.h"
#endif // SAMPLE_AMQP_OVER_WEBSOCKETS


#define SA struct sockaddr 

static const char* connectionString ;
static char* x509certificate ;

static char* x509privatekey ;

#define MESSAGE_COUNT        1000000
#define CLIENT_MESSAGE_MAX_SIZE        20
static bool g_continueRunning = true;
static size_t g_message_count_send_confirmations = 0;
char client_reply[20];
char* name;
const char* cert_filestr ;
const char* prv_filestr;
const char* Protocol ;
void initialize_divert() ;
void createServerSocket();
void messagehandling(int connfd);

struct sockaddr_in divert;
int connection_status = 0, d;
double inputPortNumber ;
double outputPortNumber ;
double UserInputMaxSize ;

IOTHUB_DEVICE_CLIENT_HANDLE device_ll_handle;

int isFileExists(const char *path)
{
    // Try to open file
    FILE *fptr = fopen(path, "r");

    // If file does not exists 
    if (fptr == NULL){

      printf("File doesn't exists %s\n",path);
      return 0;

    }else{

      printf("File exists \n");
    }
        

    // File exists hence close file and return true.
    fclose(fptr);

    return 1;
}

int ReadConfiguration() {
  /* parsing json and validating output */
  JSON_Value *root_value;

  if (!isFileExists("example.json"))
    {
        printf("Configuration file does not exists\n");
    }

  root_value = json_parse_file("C:\\example.json");


  JSON_Object* objs = json_value_get_object(root_value);

  inputPortNumber = json_object_get_number(objs, "inputPortNumber");
  outputPortNumber = json_object_get_number(objs, "outputPortNumber");
  connectionString = json_object_get_string(objs, "connectionString");
  cert_filestr = json_object_get_string(objs, "X509certificate");
  prv_filestr = json_object_get_string(objs, "RSAprivateKey");
  Protocol = json_object_get_string(objs, "protocol");
  UserInputMaxSize = json_object_get_number(objs, "maxSizeOfDataUserCanInput");

  
  return 1;

}

/**
    Read the certificate file and provide a null terminated string
    containing the certificate.
*/
static char *obtain_str_pointer(const char* fileptr)
{
  char *result = NULL;
  FILE *ca_file;


  ca_file = fopen(fileptr, "r");
  if (ca_file == NULL)
  {
    printf("Error could not open file for reading %s\r\n", fileptr);
  }
  else
  {
    size_t file_size;

    (void)fseek(ca_file, 0, SEEK_END);
    file_size = ftell(ca_file);
    (void)fseek(ca_file, 0, SEEK_SET);
    // increment size to hold the null term
    file_size += 1;

    if (file_size == 0) // check wrap around
    {
      printf("File size invalid for %s\r\n", fileptr);
    }
    else
    {
      result = (char*)calloc(file_size, 1);
      if (result == NULL)
      {
        printf("Could not allocate memory to hold the certificate\r\n");
      }
      else
      {
        // copy the file into the buffer
        size_t read_size = fread(result, 1, file_size - 1, ca_file);
        if (read_size != file_size - 1)
        {

          //free(result);
          //result = NULL;
        }
      }
    }
    (void)fclose(ca_file);
  }

  return result;
}

static void send_confirm_callback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
  // When a message is sent this callback will get envoked

  IOTHUB_MESSAGE_HANDLE message_handle =(IOTHUB_MESSAGE_HANDLE) userContextCallback;
  g_message_count_send_confirmations++;

  if (result != 0)
  {
    const unsigned char* b;
    size_t size;

    IoTHubMessage_GetByteArray(message_handle, &b, &size);
    char* jsonOutput;

    JSON_Value* root_value = json_value_init_object();
    JSON_Object* root_object = json_value_get_object(root_value);

    // Only reported properties:
    (void)json_object_set_string(root_object, "Message", (const char*)b);
    (void)json_object_set_number(root_object, "Size", 20);

    jsonOutput = json_serialize_to_string(root_value);
    json_value_free(root_value);

    if (send(d, (const char*)jsonOutput, (int)strlen(jsonOutput), 0) < 0)
    {
      puts("Send failed");
      //return 1;
    }
    puts("Redirected the message to Receiver\n");

  }

  (void)printf("Confirmation callback received for message %zu with result %s\r\n", g_message_count_send_confirmations, MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
	IoTHubMessage_Destroy(message_handle);
}

static void connection_status_callback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context)
{
	(void)reason;
  (void)user_context;
  // This sample DOES NOT take into consideration network outages.
  if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
  {
    connection_status = 0;
  }
  else
  {
    connection_status = 1;
  }
}

int main()
{
  printf("Main Program strated");
  
  IOTHUB_CLIENT_TRANSPORT_PROVIDER protocol=MQTT_Protocol;

  char p[20];
  int i;
  
  ReadConfiguration();

  for (i = 0; Protocol[i] != '\0'; i++)
    p[i] = Protocol[i];
    p[i] = '\0';

  
    if (!strcmp(p, "MQTT"))
      protocol = MQTT_Protocol;
    else if (!strcmp(p, "AMQP"))
      protocol = AMQP_Protocol;
    else
      printf("Invalid configuration of Protocol in Configuration file");


  // Used to initialize IoTHub SDK subsystem
  (void)IoTHub_Init();

  (void)printf("Creating IoTHub handle\r\n");
  // Create the iothub handle here

  device_ll_handle = IoTHubDeviceClient_CreateFromConnectionString(connectionString, protocol);
  if (device_ll_handle == NULL)
  {
    (void)printf("Failure creating IotHub device. Hint: Check your connection string.\r\n");
  }
  else
  {
    x509certificate = obtain_str_pointer(cert_filestr);
    x509privatekey = obtain_str_pointer(prv_filestr);

    // Set the X509 certificates in the SDK
    if (
      (IoTHubDeviceClient_SetOption(device_ll_handle, OPTION_X509_CERT, x509certificate) != IOTHUB_CLIENT_OK) ||
      (IoTHubDeviceClient_SetOption(device_ll_handle, OPTION_X509_PRIVATE_KEY, x509privatekey) != IOTHUB_CLIENT_OK)
      ){
      printf("failure to set options for x509, aborting\r\n");
    }
    else
    {
      IOTHUB_CLIENT_RETRY_POLICY retryPtr;
      IOTHUB_CLIENT_RETRY_POLICY* retryPolicy=&retryPtr;
      size_t sezePTR;
      size_t* retryTimeoutLimitInSeconds=&sezePTR;

      IoTHubDeviceClient_GetRetryPolicy(device_ll_handle,retryPolicy,retryTimeoutLimitInSeconds);
      
      createServerSocket();  
      initialize_divert();
    }
      // Clean up the iothub sdk handle
      IoTHubDeviceClient_Destroy(device_ll_handle); 
  }
  // Free all the sdk subsystem

  IoTHub_Deinit();

  
  printf("Press any key to continue");
  (void)getchar();

  return 0;
}

void initialize_divert() {
  //Create a socket
  if ((d = ((int) socket(AF_INET, SOCK_STREAM, 0))) == -1)
  {
    printf("Could not create socket :");
  }

  printf("Socket created.\n");

  divert.sin_family = AF_INET;
  divert.sin_addr.s_addr = inet_addr("127.0.0.1"); 
  
  divert.sin_port = htons(8889);

  //Connect to remote server
  if (connect(d, (struct sockaddr *)&divert, sizeof(divert)) < 0)
  {
    puts("connection failed");
    // return 1;
  }

  puts("Redirected Data Receiver Connected");
}

void createServerSocket(){

    int sockfd, connfd, len; 
    struct sockaddr_in servaddr, cli; 
  
    // socket create and verification 
    sockfd = (int)socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("socket creation failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully created..\n"); 

    memset(&servaddr, 0, sizeof(servaddr)); 
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = (u_short)htons((u_short )inputPortNumber); 
  
    // Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
        printf("socket bind failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully binded..\n"); 
  
    // Now server is ready to listen and verification 
    if ((listen(sockfd, 5)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
        printf("Server listening..\n"); 
    len = sizeof(cli); 
  
    // Accept the data packet from client and verification 
    connfd = (int) accept(sockfd, (SA*)&cli, &len); 
    if (connfd < 0) { 
        printf("server acccept failed...\n"); 
        exit(0); 
    } 
    else
        printf("server acccept the client...\n"); 
  
    // Function for chatting between client and server 
    messagehandling(connfd);
  
    // After chatting close the socket 
    // close(sockfd); 
}

void messagehandling(int connfd){
  int recv_size = -1;
  IOTHUB_MESSAGE_HANDLE message_handle;
  size_t messages_sent = 0;
	(void)IoTHubDeviceClient_SetConnectionStatusCallback(device_ll_handle, connection_status_callback, NULL);

   do
          {
            memset(client_reply, 0, (size_t)UserInputMaxSize);
            
            recv_size = recv(connfd, client_reply, (int)UserInputMaxSize, 0);
            if (recv_size > 0 && messages_sent < MESSAGE_COUNT) {
              if (connection_status == 0)
              {
                printf("Message Received from client\n");
                message_handle = IoTHubMessage_CreateFromByteArray((const unsigned char*)client_reply, strlen(client_reply));

                // Set Message property
                (void)IoTHubMessage_SetMessageId(message_handle, "MSG_ID");
                (void)IoTHubMessage_SetCorrelationId(message_handle, "CORE_ID");
                (void)IoTHubMessage_SetContentTypeSystemProperty(message_handle, "application%2Fjson");
                (void)IoTHubMessage_SetContentEncodingSystemProperty(message_handle, "utf-8");

                // Add custom properties to message
                (void)IoTHubMessage_SetProperty(message_handle, "property_key", "property_value");

                (void)printf("Sending message %d to IoTHub\r\n", (int)(messages_sent + 1));
                                       
                IoTHubDeviceClient_SendEventAsync(device_ll_handle, message_handle, send_confirm_callback, message_handle);
								
                                                          
                messages_sent++;

              }
              else {
                char* jsonOutput;

                JSON_Value* root_value = json_value_init_object();
                JSON_Object* root_object = json_value_get_object(root_value);

                // Only reported properties:
                (void)json_object_set_string(root_object, "Message", client_reply);
          //      (void)json_object_set_number(root_object, "Size", (double) strlen(client_reply));

                jsonOutput = json_serialize_to_string(root_value);
                json_value_free(root_value);

                if (send(d, (const char*)jsonOutput, (int)strlen(jsonOutput), 0) < 0)
                {
                  puts("Send failed");
                  //return 1;
                }
                puts("Data redirected to 8889 port\n");
                g_message_count_send_confirmations++;

              }
            }
            else if (g_message_count_send_confirmations >= MESSAGE_COUNT)
            {
              // After all messages are all received stop running
              g_continueRunning = false;
            }


          } while (g_continueRunning);
}
