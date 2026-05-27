#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
//필요한 라이브러리들 전부 import
#define PORT 8080 //8080 포트로 개방
#define BUFFER_SIZE 4096

//  댓글 노드 구조체
typedef struct Comment {
    char address[128];
    char author[64];
    char content[512];
    char timestamp[64];
    int child_count;
    struct Comment* next;
} Comment;

Comment* head = NULL;
int root_count = 0;

// JSON에서 데이터 가져오는 함수
void extract_json_value(const char* json, const char* key, char* output) {
    char search_key[64];
    sprintf(search_key, "\"%s\":\"", key);
    char* pos = strstr(json, search_key);
    
    if (pos) {
        pos += strlen(search_key);
        int i = 0;
        while (*pos != '\0' && *pos != '"' && i < 510) {
            output[i++] = *pos++;
        }
        output[i] = '\0';
    } else {
        output[0] = '\0';
    }
}

// linked_list를 이용한 댓글을 넣는 함수
void add_comment(const char* parent_addr, const char* author, const char* content) {
    char new_address[128];

    if (strlen(parent_addr) == 0) {
        root_count++;
        sprintf(new_address, "%d", root_count);
    } else {
        Comment* current = head;
        while (current != NULL) {
            if (strcmp(current->address, parent_addr) == 0) break;
            current = current->next;
        }
        if (current == NULL) return; 

        current->child_count++;
        sprintf(new_address, "%s.%d", current->address, current->child_count);
    }

    Comment* new_node = (Comment*)malloc(sizeof(Comment));
    strcpy(new_node->address, new_address);
    strcpy(new_node->author, author);
    strcpy(new_node->content, content);
    
    // 현재 시간을 MM.DD HH:MM:SS  형식으로 생성하여 저장
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    sprintf(new_node->timestamp, "%02d.%02d %02d:%02d:%02d", 
            tm_info->tm_mon + 1, tm_info->tm_mday, 
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

    new_node->child_count = 0;
    new_node->next = NULL;

    if (head == NULL) {
        head = new_node;
    } else {
        Comment* temp = head;
        while (temp->next != NULL) temp = temp->next;
        temp->next = new_node;
    }
    printf("[SERVER] 등록: %s (%s) - %s [%s]\n", new_address, author, content, new_node->timestamp);
}

// 댓글 삭제
void delete_comment(const char* target_address) {
    Comment* current = head;
    Comment* prev = NULL;
    int len = strlen(target_address);

    while (current != NULL) {
        // 주소가 완전히 일치하거나, 해당 주소로 시작하는 하위 대댓글인 경우
        if (strncmp(current->address, target_address, len) == 0 &&
            (current->address[len] == '\0' || current->address[len] == '.')) { // 특정 숫자 뒤에 아무것도 없거나 -> \0 혹은 이제 뒤에 .이 있는거만 삭제하기 (사유 : 댓글의 라벨이 1인걸 지울 경우라고 가정하면 10 또한 제거의 타깃이 되기 때문에 1을 삭제할때 1과 
            
            Comment* temp = current;
            
            // 리스트에서 노드 분리
            if (prev == NULL) {
                head = current->next;
                current = head;
            } else {
                prev->next = current->next;
                current = prev->next;
            }
            
            printf("[SERVER] 삭제: %s\n", temp->address);
            free(temp); // 메모리 해제
        } else {
            // 삭제 조건에 안 맞으면 다음 노드로 이동
            prev = current;
            current = current->next;
        }
    }
}

// 4. JSON 문자열 변환
void generate_json_response(char* buffer) {
    strcpy(buffer, "{\"data\":[");
    Comment* current = head;
    while (current != NULL) {
        char node_json[1024];
        sprintf(node_json, "{\"address\":\"%s\",\"author\":\"%s\",\"content\":\"%s\",\"timestamp\":\"%s\",\"child_count\":%d}",
                current->address, current->author, current->content, current->timestamp, current->child_count);
        strcat(buffer, node_json);
        
        current = current->next;
        if (current != NULL) strcat(buffer, ",");
    }
    strcat(buffer, "]}");
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed"); exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt"); exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed"); exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }

    printf("C Backend Server is running on port %d...\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept"); continue;
        }

        read(new_socket, buffer, BUFFER_SIZE);

        if (strncmp(buffer, "GET /comments/api/raw", 21) == 0) {
            char json_body[BUFFER_SIZE];
            generate_json_response(json_body);
            
            char http_response[BUFFER_SIZE + 512];
            sprintf(http_response, "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\n\r\n%s", json_body);
            write(new_socket, http_response, strlen(http_response));
        }
        else if (strncmp(buffer, "POST /comments/api/add", 22) == 0) {
            char* body = strstr(buffer, "\r\n\r\n");
            if (body) {
                body += 4;
                char parent_address[128], author[64], content[512];
                extract_json_value(body, "parent_address", parent_address);
                extract_json_value(body, "author", author);
                extract_json_value(body, "content", content);
                add_comment(parent_address, author, content);
            }
            const char* success_response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"success\"}";
            write(new_socket, success_response, strlen(success_response));
        }
        // 삭제 엔드포인트 추가
        else if (strncmp(buffer, "POST /comments/api/delete", 25) == 0) {
            char* body = strstr(buffer, "\r\n\r\n");
            if (body) {
                body += 4;
                char target_address[128];
                extract_json_value(body, "address", target_address);
                delete_comment(target_address);
            }
            const char* success_response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"success\"}";
            write(new_socket, success_response, strlen(success_response));
        }

        close(new_socket);
        memset(buffer, 0, BUFFER_SIZE);
    }
    return 0;
}
