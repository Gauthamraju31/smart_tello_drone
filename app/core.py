from concurrent.futures import thread
import time
import cv2
import numpy as np
import utils
import mediapipe as mp
from djitellopy import Tello
from types import FunctionType
import threading

mp_face_detection = mp.solutions.face_detection
# mp_face_drawing = mp.solutions.drawing_utils
 
class FaceTracker(mp_face_detection.FaceDetection):
      
  def __init__(self, model_selection=0, min_detection_confidence=0.5):
    super().__init__(model_selection=model_selection, min_detection_confidence=min_detection_confidence)
    self.__tracker = utils.Sort()
        
  def __draw_detection(self, image, detection):
    mp.solutions.drawing_utils.draw_detection(image, detection)
    
  # def __extract_center(self, tracked_detection):
  #   cx = (tracked_detection[0]+tracked_detection[2])/2
  #   cy = (tracked_detection[1]+tracked_detection[3])/2
  #   # tracked_detection.extend(cx,cy)
  #   np.append(tracked_detection, [cx, cy])
  #   print("Center - ", cx, cy)
  #   return tracked_detection
  
  def __extract_center(self, tracked_detections):
    cx = (tracked_detections[:,0]+tracked_detections[:,2])/2
    cy = (tracked_detections[:,1]+tracked_detections[:,3])/2
    print(cx.shape,cy.shape)
    
    return np.append(np.append(tracked_detections,cx.reshape(-1,1), axis=1),cy.reshape(-1,1),axis=1)
    
  def track(self, image, draw=True):
        
    # pass by reference.
    image.flags.writeable = False
    image = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
    results = self.process(image)
    image.flags.writeable = True
    image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
    
    if results.detections:
      # Convert result to numpy for tracking
      faces = []
      for detection in results.detections:
        xmin = utils.zero_clip(detection.location_data.relative_bounding_box.xmin)
        ymin = utils.zero_clip(detection.location_data.relative_bounding_box.ymin)
        xmax = xmin + detection.location_data.relative_bounding_box.width
        ymax = ymin + detection.location_data.relative_bounding_box.height
        faces.append([xmin,ymin,xmax,ymax,detection.score])
      
      # Track the detections
      tracked_faces = self.__tracker.update(np.array(faces))

      # Draw
      if draw:
        for detection in results.detections:
          self.__draw_detection(image, detection)
    else:
      tracked_faces = self.__tracker.update()
      
    # tracked_faces = list(map(self.__extract_center, tracked_faces))
    tracked_faces = self.__extract_center(tracked_faces)
    print("Tracked faces", type(tracked_faces), tracked_faces)
  
    return image, tracked_faces
      
class TelloSmartController(Tello):
      
  def __init__(self):
    super().__init__()
    self.rescale = False
    self.rescale_width = 720
    self.rescale_height = 1280
    self.frame_reader = None
    self.precision = 20
    
    self.controlkeys = {
      "w": self.move_forward,
      "a": self.move_left,
      "s": self.move_back,
      "d": self.move_right,
      "q": self.rotate_clockwise,
      "e": self.rotate_counter_clockwise,
      "v": self.move_up,
      "c": self.move_down,
      "t": self.takeoff,
      "l": self.land,
      "f": self.emergency
    }
    self.facetracker = None
    self.automode = False
    self.viz_thread = None
    self.followface = False
    self.AutoModePID = {"pan": None, "tilt": None}
    self.max_speed_threshold = 40
    
  def enable_streaming(self):
    self.streamon()
    self.frame_reader = self.get_frame_read()
    
  def disable_streaming(self):
    self.streamoff()
    self.frame_reader = None
    
  def enable_facefollow(self):
    self.AutoModePID["pan"] = utils.PID(kP=0.7, kI=0.0001, kD=0.1)
    self.AutoModePID["tilt"] = utils.PID(kP=0.7, kI=0.0001, kD=0.1)
    self.AutoModePID["pan"].initialize()
    self.AutoModePID["tilt"].initialize()
    
  def update_rescale_params(self, width, height):
    self.rescale_width = width
    self.rescale_height = height
  
  def capture(self):
    assert self.frame_reader is not None
    frame = self.frame_reader.frame
    if self.rescale:
      cv2.resize(frame, (self.rescale_width, self.rescale_height))
    if self.facetracker is not None:
      frame, tracked_faces = self.facetracker.track(frame)
    if self.followface:
      cx = frame.shape[1]//2
      cy = frame.shape[0]//2
      cv2.circle(frame, center=(cx,cy),radius=5,color=(0,0,255),thickness=-1)
      if tracked_faces.shape[0]:
        # Find face with smallest id
        minIdx = tracked_faces[:,4].argmin()
        fx = int(tracked_faces[minIdx,5]*frame.shape[1])
        fy = int(tracked_faces[minIdx,6]*frame.shape[0])
        cv2.arrowedLine(frame, (cx, cy), (fx, fy), color=(0, 255, 0), thickness=2)
        cv2.putText(frame, str(tracked_faces[minIdx,4]), (fx, fy), cv2.FONT_HERSHEY_SIMPLEX, 1, 
                    (0, 255, 0), 2, cv2.LINE_AA)
        pan_error = cx - fx
        tilt_error = cy - fy
        pan_update = self.AutoModePID["pan"].update(pan_error, sleep=0)
        tilt_update = self.AutoModePID["tilt"].update(tilt_error, sleep=0)
        
        #cv2.putText(frame, f"X Error: {pan_error} PID: {pan_update:.2f}", (20, 30), cv2.FONT_HERSHEY_SIMPLEX, 1,(0, 255, 0), 2, cv2.LINE_AA)

        #cv2.putText(frame, f"Y Error: {tilt_error} PID: {tilt_update:.2f}", (20, 70), cv2.FONT_HERSHEY_SIMPLEX,1,(0, 0, 255), 2, cv2.LINE_AA)

        if pan_update > self.max_speed_threshold:
            pan_update = self.max_speed_threshold
        elif pan_update < -self.max_speed_threshold:
            pan_update = -self.max_speed_threshold

        # NOTE: if face is to the right of the drone, the distance will be negative, but
        # the drone has to have positive power so I am flipping the sign
        pan_update = pan_update * -1

        if tilt_update > self.max_speed_threshold:
            tilt_update = self.max_speed_threshold
        elif tilt_update < -self.max_speed_threshold:
            tilt_update = -self.max_speed_threshold

        print(int(pan_update), int(tilt_update))
        # if track_face and fly:
        # left/right: -100/100
        # self.send_rc_control(int(pan_update // 3), 0, int(tilt_update) // 2, 0)
    return frame
  
  def keycontrol(self, key):
    if key in self.controlkeys:
      self.controlkeys[key](self.precision)
    else:
      pass
    
  # Smart features
  def enable_facetracker(self):
    self.facetracker = FaceTracker()
      
  def disable_facetracker(self):
    del(self.facetracker)
    self.facetracker = None
    
  def start_following_face(self):
    self.automode = True
    self.followface = True
    self.enable_facetracker()
    self.enable_facefollow()
    
  def stop_following_face(self):
    self.automode = False
    self.followface = False
    self.disable_facetracker()
  
  @staticmethod
  def tello_methods():
    """
    Returns callable methods
    Internal method
    """
    tmethods = [func for func in dir(TelloSmartController) if callable(getattr(TelloSmartController, func)) and not func.startswith("__") ]
    tmethods = [func for func in tmethods if "Internal" not in str(getattr(TelloSmartController, func).__doc__)]
    return tmethods
  
  def gui(self):
    while True:
      frame = self.capture()
      cv2.imshow("Tello Frame", frame)
      if cv2.waitKey(10) & 0xFF == 27:
        break
    cv2.destroyAllWindows()
    
      
# Testing standard camera
class Camera(cv2.VideoCapture):
      
  def __init__(self, uri="/dev/video0", fps=24, rescale=False, rescale_width=720, rescale_height=1280):
    super().__init__(self, uri)
    self.rescale = rescale
    self.rescale_width = rescale_width
    self.rescale_height = rescale_height
    self.frame_num = 0
    self.Open()
    
  def read_frame(self):
    if self.isOpen():
      success, frame = self.read()
      if not success:
          return None
      else:
          if self.rescale:
            frame = cv2.resize(frame, self.rescale_width, self.rescale_height)
          return frame
    else:
      return None
