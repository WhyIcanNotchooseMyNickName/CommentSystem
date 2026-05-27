#!/bin/bash

# 1. 설정 변수 정의
PORT=8080
SOURCE_FILE="server.c"
OUTPUT_BINARY="comment_server"
LOG_FILE="server.log"

echo "=========================================="
echo "이산구조 댓글 시스템 배포 스크립트를 시작합니다."
echo "=========================================="

# 2. 기존에 실행 중인 C 서버 프로세스 종료
echo "[1/3] 기존 서버 프로세스를 확인하고 종료합니다..."

# 8080 포트를 사용 중인 프로세스의 PID를 찾음
PID=$(lsof -t -i:$PORT)

if [ -n "$PID" ]; then
    echo "포트 $PORT번에서 실행 중인 프로세스(PID: $PID)를 강제 종료합니다."
    kill -9 $PID
    sleep 1  # 프로세스가 완전히 종료될 때까지 잠시 대기
else
    echo "포트 $PORT번에서 실행 중인 기존 프로세스가 없습니다."
fi

# 3. C 백엔드 코드 재컴파일
echo "[2/3] C 소스 코드를 컴파일합니다 ($SOURCE_FILE -> $OUTPUT_BINARY)..."
gcc $SOURCE_FILE -o $OUTPUT_BINARY

# 컴파일 성공 여부 체크 ($?는 직전 명령어의 결과 코드, 0이면 성공)
if [ $? -eq 0 ]; then
    echo "컴파일 성공!"
else
    echo "❌ 오류: 컴파일에 실패했습니다. server.c 코드를 확인해주세요."
    exit 1
fi

# 4. 새 서버를 백그라운드에서 실행 (SSH 세션이 끊겨도 유지되도록 nohup 사용)
echo "[3/3] 새 서버를 백그라운드에서 실행합니다..."
nohup ./$OUTPUT_BINARY > $LOG_FILE 2>&1 &

# 잠시 대기 후 프로세스가 정상적으로 떴는지 포트 체크
sleep 1
NEW_PID=$(lsof -t -i:$PORT)

if [ -n "$NEW_PID" ]; then
    echo "=========================================="
    echo "🎉 배포가 성공적으로 완료되었습니다!"
    echo "새 프로세스 PID: $NEW_PID (포트: $PORT)"
    echo "서버 실시간 출력(로그)은 $LOG_FILE 에서 확인 가능합니다."
    echo "=========================================="
else
    echo "❌ 오류: 서버가 정상적으로 시작되지 않았습니다. $LOG_FILE 파일을 확인하세요."
fi
