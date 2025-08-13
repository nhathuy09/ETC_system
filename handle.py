
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from paddleocr import PaddleOCR
from ultralytics import YOLO
import cv2
import numpy as np
import mysql.connector
import requests
from datetime import datetime
import traceback
from pathlib import Path
import time

app = FastAPI()
yolo_model = YOLO(r"C:\Users\Vo Nhat Huy\Documents\yolo_py\runs\detect\train7\weights\best.pt")
ocr = PaddleOCR(use_angle_cls=False, lang='en', show_log=False)
PROCESSED_FOLDER = Path(r"C:\Users\Vo Nhat Huy\Documents\processed_images")

db = mysql.connector.connect(
    host="localhost",
    user="root",
    password="VoNhatHuy3175",
    database="TramThuPhi"
)
cursor = db.cursor()

vehicle_fees = {
    "Car": 10000,
    "Truck": 20000,
    "Motorbike": 5000
}
def save_processed_image(image_np, plate_number, confidence):
    today_str = datetime.now().strftime("%Y-%m-%d")
    daily_folder = PROCESSED_FOLDER / today_str
    daily_folder.mkdir(parents=True, exist_ok=True)

    filename = f"{plate_number}_{int(time.time())}.jpg"
    save_path = daily_folder / filename

    if not cv2.imwrite(str(save_path), image_np):
        print("Lưu ảnh thất bại:", save_path)
    else:
        print("Ảnh đã lưu:", save_path)

    return str(save_path)
def log_to_database(source, content):
    try:
        cursor.execute("INSERT INTO logs (source, content) VALUES (%s, %s)", (source, content))
        db.commit()
    except Exception as e:
        print("Lỗi ghi log:", e)

def insert_transaction(plate, fee, balance_after, result, message):
    cursor.execute("""
            INSERT INTO transactions (plate_number, fee, balance_after, result, message)
            VALUES (%s, %s, %s, %s, %s)
        """, (plate, fee, balance_after, result, message))
    db.commit()
def insert_images(plate_number,image_path,confidence):
    cursor.execute("INSERT INTO images(plate_number,image_path,confidence) VALUES(%s,%s,%s)",(plate_number,image_path,confidence))
    db.commit()
def notify_node_red(plate_number, fee):
    try:
        response = requests.post(
            "http://10.0.18.184:1880/open_gate",
            json={"plate_number": plate_number, "fee": fee},
            timeout=3
        )
        if response.status_code == 200:
            log_to_database("Notify", f"Gửi mở cổng: {plate_number}")
        else:
            print("Lỗi Node-RED:", response.status_code)
    except Exception as e:
        print("Không gửi được Node-RED:", e)

def send_plate_to_server(plate_number, status="unknown", message=""):
    try:
        response = requests.post(
            "http://10.0.18.184:1880/report_plate",
            json={"plate_number": plate_number, "status": status, "message": message},
            timeout=3
        )
        if response.status_code == 200:
            log_to_database("Report", f"Đã gửi {plate_number} - {status}")
    except Exception as e:
        print(f"Lỗi gửi plate lên server: {e}")

def detect_and_show_plates_from_np(image_np, conf_thresh=0.3):
    response = {
        "status": "error",
        "plate": None,
        "fee": 0,
        "balance_after": None,
        "message": ""
    }
    try:
        results = yolo_model(image_np)[0]
        boxes = results.boxes.xyxy.cpu().numpy()
        scores = results.boxes.conf.cpu().numpy()
       
        found = False
        plate_texts = []

        for box, score in zip(boxes, scores):
            if score < conf_thresh:
                continue
            x1, y1, x2, y2 = map(int, box)
            plate_crop = image_np[y1:y2, x1:x2]
            found = True
            ocr_results = ocr.ocr(plate_crop)
            confidence = float(score)
            text_line = ""
            for line in ocr_results:
                for _, (text, conf) in line:
                    if conf > 0.5:
                        text_line += text + " "

            if text_line.strip():
                raw_plate = ''.join(text_line.strip().split())
                clean_plate = ''.join(filter(str.isalnum, raw_plate))
                # image_path = save_processed_image(image_np, clean_plate, confidence)
                if not clean_plate:
                    send_plate_to_server("unknown", status="unreadable", message="Không nhận diện được biển số")
                    return {"status": "error", "message": "Không nhận diện được biển số"}

                    # Kiểm tra DB
                image_path = save_processed_image(image_np, clean_plate, confidence)
                response["plate"] = clean_plate
                insert_images(clean_plate, image_path, confidence)
                cursor.execute("SELECT * FROM vehicles WHERE plate_number = %s", (clean_plate,))
                vehicle = cursor.fetchone()
                if vehicle:
                    _, plate, owner, vtype, balance, status, _ = vehicle
                    if status == "banned":
                        insert_transaction(plate, 0, balance, "failed", "Bị chặn")
                        response["message"] = "Xe bị chặn"
                        send_plate_to_server(clean_plate, status="banned", message=response["message"])
                        # return {"status": "error", "plate": clean_plate, "message": "Xe bị chặn"}
                    else:
                        fee = vehicle_fees.get(vtype, 10000)
                        if balance >= fee:
                            new_balance = balance - fee
                            cursor.execute(
                                "UPDATE vehicles SET balance = %s WHERE plate_number=%s",
                                (new_balance, clean_plate)
                            )
                            db.commit()
                            insert_transaction(plate, fee, new_balance, "success", "Qua trạm thành công")
                            notify_node_red(clean_plate, fee)
                            send_plate_to_server(clean_plate, status="ok", message="Xe hợp lệ")
                            # return {"status": "success", "plate": clean_plate, "fee": fee, "balance_after": new_balance}
                            response.update({
                                "status": "success",
                                "fee": fee,
                                "balance_after": new_balance,
                                "message": "Qua trạm thành công"
                            })
                        else:
                            
                            insert_transaction(plate, 0, balance, "failed", "Không đủ tiền")
                            send_plate_to_server(clean_plate, status="insufficient", message="không đủ tiền")
                            response.update({
                                "status": "insufficient",
                                "fee": fee,
                                "balance_after":balance ,
                                "message": "Không đủ tiền"
                            })
                            # return {"status": "error", "plate": clean_plate, "message": "Không đủ tiền"}
                else:
                    response["message"] = "Không tồn tại trong hệ thống"
                    send_plate_to_server(clean_plate, status="not_found", message=response["message"])
                    # return {"status": "error", "plate": clean_plate, "message": "Không tồn tại trong hệ thống"}

        if not found:
            response["message"] = "Không phát hiện được biển số"
            send_plate_to_server("unknown", status="no_plate", message=response["message"])
            # return {"status": "error", "message": "Không phát hiện được biển số"}
    except Exception as e:
        traceback.print_exc()
        # return {"status": "error", "message": str(e)}
        response["message"] = str(e)
    
    return response
@app.post("/process")
async def process_image(request: Request):
    try:
        body = await request.body()
        np_img = np.frombuffer(body, np.uint8)
        image_np = cv2.imdecode(np_img, cv2.IMREAD_COLOR)

        if image_np is None:
            return JSONResponse(content={"status": "error", "message": "Ảnh không hợp lệ"}, status_code=200)

        result = detect_and_show_plates_from_np(image_np)
        return JSONResponse(content=result, status_code=200)

    except Exception as e:
        traceback.print_exc()
        return JSONResponse(content={"status": "error", "message": str(e)}, status_code=200)

@app.on_event("shutdown")
def shutdown_event():
    cursor.close()
    db.close()
