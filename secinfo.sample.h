// rename this file to secinfo.h

#ifndef FILE_FOO_SEEN
#define FILE_FOO_SEEN

// Please input the SSID and password of WiFi
const char* ssid     = "<your SSID>";
const char* password = "<your PSK>";

/*String containing Hostname, Device Id & Device Key in the format:                         */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
//static const char* connectionString = "";

static const char* connectionString = "<your connection string>";

#endif /* !FILE_FOO_SEEN */