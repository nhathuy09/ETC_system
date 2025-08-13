from ultralytics import YOLO

# model = YOLO("yolo11n.yaml")

model = YOLO("yolo11n.pt")


results = model.train(data="C:\\Users\\Vo Nhat Huy\\Documents\\yolo_py\\data.yaml", epochs=50)

# results = model.val()


# results = model("https://ultralytics.com/images/bus.jpg")

# # Export the model to ONNX format
# success = model.export(format="onnx")