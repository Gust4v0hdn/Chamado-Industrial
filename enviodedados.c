#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <winsock2.h>
#include <windows.h>
#include <lmcons.h>
#include <iphlpapi.h>
#include <wininet.h>
#include <ctype.h>
#include "cJSON.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")

#define PORT 5000
#define TOKEN "8086463894:AAF7p-AqnWSO2YqHQdeL5EppGOuAbbEqi6s"
#define CHAT_ID "-4799484557"
#define HTTP_URL "http://172.16.41.154/buzz?"

const char *IPS[] = {
    "172.16.41.211",
    "172.16.41.247",
    "172.16.41.64",
};
#define TOTAL_IPS (sizeof(IPS)/sizeof(IPS[0]))

// Salva mensagem no log
void salvarLog(const char *mensagem) {
    FILE *log = fopen("registro.log", "a");
    if (log != NULL) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s\n",
                t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute, t.wSecond, mensagem);
        fclose(log);
    }
}

void salvarLogf(const char *formato, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, formato);
    vsnprintf(buffer, sizeof(buffer), formato, args);
    va_end(args);
    salvarLog(buffer);
}

BOOL arquivoExiste(const char *caminho) {
    DWORD attr = GetFileAttributesA(caminho);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

BOOL redeDisponivel(void) {
    ULONG outBufLen = 0;
    DWORD dwRetVal = GetAdaptersAddresses(AF_INET, 0, NULL, NULL, &outBufLen);
    if (dwRetVal != ERROR_BUFFER_OVERFLOW) {
        salvarLogf("[REDE] FALHA: Erro ao obter tamanho de buffer (%lu)", dwRetVal);
        return FALSE;
    }

    IP_ADAPTER_ADDRESSES *adapters = (IP_ADAPTER_ADDRESSES *) malloc(outBufLen);
    if (!adapters) {
        salvarLog("[REDE] FALHA: Falha na alocacao de memoria para adaptadores.");
        return FALSE;
    }

    dwRetVal = GetAdaptersAddresses(AF_INET, 0, NULL, adapters, &outBufLen);
    if (dwRetVal != NO_ERROR) {
        free(adapters);
        salvarLogf("[REDE] FALHA: GetAdaptersAddresses retornou erro %lu", dwRetVal);
        return FALSE;
    }

    for (IP_ADAPTER_ADDRESSES *adapter = adapters; adapter != NULL; adapter = adapter->Next) {
        for (IP_ADAPTER_UNICAST_ADDRESS *addr = adapter->FirstUnicastAddress; addr != NULL; addr = addr->Next) {
            SOCKADDR_IN *sa_in = (SOCKADDR_IN *) addr->Address.lpSockaddr;
            DWORD ip = ntohl(sa_in->sin_addr.S_un.S_addr);
            if (ip != 0 && (ip >> 24) != 169) {
                free(adapters);
                salvarLog("[REDE] IP válido encontrado.");
                return TRUE;
            }
        }
    }

    free(adapters);
    salvarLog("[REDE] Nenhum IP válido encontrado.");
    return FALSE;
}

char *url_encode(const char *str) {
    if (str == NULL) return NULL;
    char *enc = malloc(strlen(str) * 3 + 1);
    if (enc == NULL) return NULL;
    char *penc = enc;
    const char *pstr = str;
    while (*pstr) {
        if (isalnum((unsigned char)*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') {
            *penc++ = *pstr;
        } else {
            sprintf(penc, "%%%02X", (unsigned char)*pstr);
            penc += 3;
        }
        pstr++;
    }
    *penc = '\0';
    return enc;
}

BOOL enviarTelegram(const char *mensagem) {
    salvarLog("[TELEGRAM] Enviando mensagem...");
    char url[1024];
    char *msgEncoded = url_encode(mensagem);
    if (msgEncoded == NULL) {
        salvarLog("[TELEGRAM] FALHA: Erro ao codificar mensagem.");
        return FALSE;
    }
    sprintf(url, "https://api.telegram.org/bot%s/sendMessage?chat_id=%s&text=%s", TOKEN, CHAT_ID, msgEncoded);
    free(msgEncoded);
    HINTERNET hInternet = InternetOpen("Telegram", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        salvarLog("[TELEGRAM] FALHA: Nao foi possivel abrir a conexao com a internet.");
        return FALSE;
    }
    HINTERNET hUrl = InternetOpenUrl(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        salvarLog("[TELEGRAM] FALHA: Nao foi possivel abrir a URL da API.");
        return FALSE;
    }
    char responseBuffer[1024];
    DWORD bytesRead;
    BOOL apiSuccess = InternetReadFile(hUrl, responseBuffer, sizeof(responseBuffer) - 1, &bytesRead);
    responseBuffer[bytesRead] = '\0';
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    if (apiSuccess && strstr(responseBuffer, "\"ok\":true") != NULL) {
        salvarLog("[TELEGRAM] OK: Mensagem enviada com sucesso.");
        return TRUE;
    } else {
        salvarLog("[TELEGRAM] FALHA: Resposta da API indica erro.");
        salvarLog(responseBuffer);
        return FALSE;
    }
}

typedef struct {
    const char *ip;
    char *mensagem;
} ThreadData;

DWORD WINAPI threadEnviarSocket(LPVOID arg) {
    ThreadData *data = (ThreadData *)arg;
    const char *ip = data->ip;
    const char *mensagem = data->mensagem;
    salvarLogf("[SOCKET] Conectando em %s...", ip);
    SOCKET sock;
    struct sockaddr_in server;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        salvarLog("[SOCKET] FALHA: Socket invalido.");
        free(data->mensagem);
        free(data);
        return 1;
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(ip);
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == 0) {
        send(sock, mensagem, (int)strlen(mensagem), 0);
        salvarLogf("[SOCKET] OK: Enviado para %s", ip);
    } else {
        salvarLogf("[SOCKET] FALHA: Falha ao conectar em %s", ip);
    }
    closesocket(sock);
    free(data->mensagem);
    free(data);
    return 0;
}

BOOL enviarParaTodosIPs(const char *mensagem) {
    HANDLE threads[TOTAL_IPS];
    BOOL algumPingOk = FALSE;
    for (int i = 0; i < TOTAL_IPS; i++) {
        ThreadData *data = malloc(sizeof(ThreadData));
        if (data == NULL) continue;
        data->ip = IPS[i];
        data->mensagem = strdup(mensagem);
        if (data->mensagem == NULL) {
            free(data);
            continue;
        }
        threads[i] = CreateThread(NULL, 0, threadEnviarSocket, (LPVOID)data, 0, NULL);
        if (threads[i] != NULL) {
            algumPingOk = TRUE;
        }
    }
    WaitForMultipleObjects(TOTAL_IPS, threads, TRUE, INFINITE);
    for (int i = 0; i < TOTAL_IPS; i++) {
        if (threads[i] != NULL) CloseHandle(threads[i]);
    }
    return algumPingOk;
}

BOOL enviarBuzzDireto(const char *mensagem) {
    salvarLog("[BUZZ] Enviando mensagem...");
    HINTERNET hInternet = InternetOpen("BuzzHTTP", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        salvarLog("[BUZZ] FALHA: Nao foi possivel abrir a conexao com a internet.");
        return FALSE;
    }

    URL_COMPONENTSA urlComp = { 0 };
    char host[256] = {0};
    char path[512] = {0};

    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path);

    if (!InternetCrackUrlA(HTTP_URL, 0, 0, &urlComp)) {
        salvarLog("[BUZZ] FALHA: Erro ao interpretar a URL.");
        InternetCloseHandle(hInternet);
        return FALSE;
    }

    HINTERNET hConnect = InternetConnectA(hInternet, host, INTERNET_DEFAULT_HTTP_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        salvarLog("[BUZZ] FALHA: Nao foi possivel conectar ao host.");
        InternetCloseHandle(hInternet);
        return FALSE;
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path, NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) {
        salvarLog("[BUZZ] FALHA: Nao foi possivel criar requisicao HTTP.");
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return FALSE;
    }

    char *msgEncoded = url_encode(mensagem);
    if (!msgEncoded) {
        salvarLog("[BUZZ] FALHA: Erro ao codificar a mensagem.");
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return FALSE;
    }

    char postData[1024];
    snprintf(postData, sizeof(postData), "msg=%s", msgEncoded);
    free(msgEncoded);

    const char *headers = "Content-Type: application/x-www-form-urlencoded";

    BOOL success = HttpSendRequestA(hRequest, headers, -1L, postData, (DWORD)strlen(postData));

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    if (success) {
        salvarLog("[BUZZ] OK: Mensagem enviada com sucesso.");
        return TRUE;
    } else {
        salvarLog("[BUZZ] FALHA: Falha ao enviar a requisicao.");
        return FALSE;
    }
}

// Atualiza o valor "STATUS" no arquivo JSON para true
void atualizarStatusJson(const char *caminho) {
    salvarLogf("[JSON] Tentando atualizar STATUS em %s...", caminho);
    FILE *arquivo = fopen(caminho, "rb");
    if (!arquivo) {
        salvarLogf("[JSON] FALHA: Nao foi possivel abrir o arquivo %s.", caminho);
        return;
    }
    fseek(arquivo, 0, SEEK_END);
    long tamanho = ftell(arquivo);
    rewind(arquivo);
    if (tamanho <= 0) {
        fclose(arquivo);
        salvarLogf("[JSON] FALHA: Arquivo %s esta vazio.", caminho);
        return;
    }
    char *buffer = malloc(tamanho + 1);
    if (!buffer) {
        fclose(arquivo);
        salvarLogf("[JSON] FALHA: Falha na alocacao de memoria para %s.", caminho);
        return;
    }
    fread(buffer, 1, tamanho, arquivo);
    buffer[tamanho] = '\0';
    fclose(arquivo);
    cJSON *json = cJSON_Parse(buffer);
    free(buffer);
    if (!json) {
        salvarLogf("[JSON] FALHA: Erro ao interpretar JSON de %s.", caminho);
        return;
    }
    cJSON *status = cJSON_GetObjectItemCaseSensitive(json, "STATUS");
    if (!status) {
        cJSON_AddItemToObject(json, "STATUS", cJSON_CreateBool(1));
        salvarLog("[JSON] Chave 'STATUS' nao encontrada. Adicionada com valor true.");
    } else {
        cJSON_SetBoolValue(status, 1);
        salvarLog("[JSON] Chave 'STATUS' encontrada. Valor alterado para true.");
    }
    char *saida = cJSON_Print(json);
    if (saida == NULL) {
        cJSON_Delete(json);
        salvarLog("[JSON] FALHA: Erro ao imprimir JSON.");
        return;
    }
    FILE *out = fopen(caminho, "w");
    if (out) {
        fprintf(out, "%s", saida);
        fclose(out);
        salvarLog("[JSON] OK: STATUS alterado e arquivo salvo.");
    } else {
        salvarLog("[JSON] FALHA: Nao foi possivel abrir o arquivo para escrita.");
    }
    cJSON_Delete(json);
    free(saida);
}

// Processa o envio de mensagens para os endpoints, ignorando o BUZZ
void processarEnvio(const char *textoFinal) {
    BOOL okTelegram = enviarTelegram(textoFinal);
    BOOL algumPingOk = enviarParaTodosIPs(textoFinal);
    salvarLog("[PROCESSO] O envio do 'BUZZ' foi ignorado.");

    if (okTelegram && algumPingOk) {
        salvarLog("[PROCESSO] Envios (Telegram e Sockets) OK. Atualizando STATUS...");
        atualizarStatusJson("mensagem.json");
    } else {
        salvarLog("[PROCESSO] Falha em algum envio (Telegram ou Sockets).");
    }
}

// --- Função Principal ---
int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        salvarLogf("[ERRO FATAL] Falha ao inicializar o Winsock: %d", WSAGetLastError());
        return 1;
    }
    salvarLog("[MAIN] Início do programa. Winsock inicializado.");
    salvarLog("[MAIN] Verificando rede e mensagem.json...");

    if (redeDisponivel() && arquivoExiste("mensagem.json")) {
        FILE *arquivo = fopen("mensagem.json", "rb");
        if (arquivo) {
            fseek(arquivo, 0, SEEK_END);
            long tamanho = ftell(arquivo);
            rewind(arquivo);
            if (tamanho > 0) {
                char *buffer = malloc(tamanho + 1);
                if (buffer) {
                    fread(buffer, 1, tamanho, arquivo);
                    buffer[tamanho] = '\0';
                    cJSON *json = cJSON_Parse(buffer);
                    free(buffer);
                    if (json) {
                        cJSON *status_item = cJSON_GetObjectItemCaseSensitive(json, "STATUS");
                        if (status_item && cJSON_IsBool(status_item) && !cJSON_IsTrue(status_item)) {
                            cJSON *mensagem = cJSON_GetObjectItemCaseSensitive(json, "SDED");
                            cJSON *data      = cJSON_GetObjectItemCaseSensitive(json, "DATA");
                            cJSON *hora      = cJSON_GetObjectItemCaseSensitive(json, "HORA");
                            cJSON *hostname = cJSON_GetObjectItemCaseSensitive(json, "HOSTNAME");

                            if (cJSON_IsString(mensagem) && cJSON_IsString(data) && cJSON_IsString(hora) && cJSON_IsString(hostname)) {
                                char nomeUsuario[UNLEN + 1];
                                DWORD nomeTamanho = UNLEN + 1;
                                GetUserNameA(nomeUsuario, &nomeTamanho);
                                char textoFinal[512];
                                snprintf(textoFinal, sizeof(textoFinal), "ERRO: %s\nUsuario: %s\nPC: %s\nData: %s - Hora: %s",
                                         mensagem->valuestring, nomeUsuario, hostname->valuestring, data->valuestring, hora->valuestring);
                                salvarLog("[MAIN] Mensagem formatada. Enviando...");
                                processarEnvio(textoFinal);
                            } else {
                                salvarLog("[MAIN] FALHA: Campos JSON obrigatorios ausentes ou invalidos.");
                            }
                        } else {
                            salvarLog("[MAIN] OK: O arquivo JSON ja tem STATUS = true. Nao e necessario enviar.");
                        }
                        cJSON_Delete(json);
                    } else {
                        salvarLog("[MAIN] FALHA: Erro ao interpretar JSON.");
                    }
                } else {
                    salvarLog("[MAIN] FALHA: Falha na alocacao de memoria.");
                }
            } else {
                salvarLog("[MAIN] FALHA: Arquivo JSON esta vazio.");
            }
            fclose(arquivo);
        }
    } else {
        salvarLog("[MAIN] Condicoes nao atendidas (rede offline ou arquivo JSON ausente).");
    }

    WSACleanup();
    salvarLog("[MAIN] Fim do programa");
    return 0;
}
