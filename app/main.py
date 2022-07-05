from core import FaceTracker
import utils
import cv2

if __name__ == "__main__":
    
    facetracker = FaceTracker()
    cap = cv2.VideoCapture(0)
    frame_num = 0
    max_speed_threshold = 40
    AutoModePID = {"pan": None, "tilt": None}
    AutoModePID["pan"] = utils.PID(kP=0.7, kI=0.0001, kD=0.1)
    AutoModePID["tilt"] = utils.PID(kP=0.7, kI=0.0001, kD=0.1)
    AutoModePID["pan"].initialize()
    AutoModePID["tilt"].initialize()
    while cap.isOpened():
        success, image = cap.read()
        if not success:
            print("Ignoring empty camera frame.")
            # If loading a video, use 'break' instead of 'continue'.
            continue
        
        image, result = facetracker.track(image)
    
        # PID Code
        cx = image.shape[1]//2
        cy = image.shape[0]//2
        cv2.circle(image, center=(cx,cy),radius=5,color=(255,255,255),thickness=-1)
        if result.shape[0]:
            # Find face with smallest id
            minIdx = result[:,4].argmin()
            print(result[minIdx,5]*image.shape[1], result[minIdx,6]*image.shape[0])
            # cv2.arrowedLine(image, (cx,cy), (int(result[minIdx,5]*image.shape[1]), int(result[minIdx,6]*image.shape[0])), color=(0, 255, 0), thickness=2)
            
            fx = int(result[minIdx,5]*image.shape[1])
            fy = int(result[minIdx,6]*image.shape[0])
            cv2.arrowedLine(image, (cx, cy), (fx, fy), color=(0, 255, 0), thickness=2)
            pan_error = cx - fx
            tilt_error = cy - fy
            pan_update = AutoModePID["pan"].update(pan_error, sleep=0)
            tilt_update = AutoModePID["tilt"].update(tilt_error, sleep=0)
            
            cv2.putText(image, f"X Error: {pan_error} PID: {pan_update:.2f}", (20, 30), cv2.FONT_HERSHEY_SIMPLEX, 1,
                        (0, 255, 0), 2, cv2.LINE_AA)

            cv2.putText(image, f"Y Error: {tilt_error} PID: {tilt_update:.2f}", (20, 70), cv2.FONT_HERSHEY_SIMPLEX,
                        1,
                        (0, 0, 255), 2, cv2.LINE_AA)

            if pan_update > max_speed_threshold:
                pan_update = max_speed_threshold
            elif pan_update < -max_speed_threshold:
                pan_update = -max_speed_threshold

            # NOTE: if face is to the right of the drone, the distance will be negative, but
            # the drone has to have positive power so I am flipping the sign
            pan_update = pan_update * -1

            if tilt_update > max_speed_threshold:
                tilt_update = max_speed_threshold
            elif tilt_update < -max_speed_threshold:
                tilt_update = -max_speed_threshold

            print(int(pan_update), int(tilt_update))
        
        cv2.imshow('MediaPipe Face Detection', image)
        if cv2.waitKey(5) & 0xFF == 27:
          break
        frame_num+=1
    cap.release()
    