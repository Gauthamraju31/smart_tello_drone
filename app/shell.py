from argparse import ArgumentError
from cmd import Cmd
from core import TelloSmartController
import inspect
import time

class TelloPrompt(Cmd, TelloSmartController):
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    
    @staticmethod
    def __title(msg):
        return TelloPrompt.HEADER+TelloPrompt.BOLD+msg+TelloPrompt.ENDC
    
    @staticmethod
    def __error(msg):
        return TelloPrompt.FAIL+"Error: "+msg+TelloPrompt.ENDC
    
    @staticmethod
    def __info(msg):
        return TelloPrompt.OKGREEN+"Info: "+msg+TelloPrompt.ENDC
    
    @staticmethod
    def __shell(msg):
        return TelloPrompt.OKCYAN+msg+TelloPrompt.ENDC
    
    intro = "\033[95m\033[1m | Welcome to Tello Shell. Type help or ? to list commands. | \n\033[0m"
    prompt = "\033[96mTello>> \033[0m"
    
    def init_tello(self):
        print(TelloPrompt.OKGREEN)
        TelloSmartController.__init__(self)
        print(TelloPrompt.ENDC)
        self.tellocontrols = TelloSmartController.tello_methods()
        
        # Auto generate tello callable functions
        # for control in self.tellocontrols:
            # exec("def do_"+control+"(cls,arg): cls."+control+"()")
            # setattr(self, "do_"+control, getattr(self,control))
            
    # def onecmd(self, line):
    #     """Interpret the argument as though it had been typed in response
    #     to the prompt.

    #     This may be overridden, but should not normally need to be;
    #     see the precmd() and postcmd() methods for useful execution hooks.
    #     The return value is a flag indicating whether interpretation of
    #     commands by the interpreter should stop.

    #     """
    #     cmd, arg, line = self.parseline(line)
    #     if not line:
    #         return self.emptyline()
    #     if cmd is None:
    #         return self.default(line)
    #     self.lastcmd = line
    #     if line == 'EOF' :
    #         self.lastcmd = ''
    #     if cmd == '':
    #         return self.default(line)
    #     else:
    #         try:
    #             func = getattr(self, 'do_' + cmd)
    #         except AttributeError:
    #             return self.default(line)
            
    #         argslist = arg.split(" ")
    #         try:
    #             if len(inspect.getfullargspec(func).args):
    #                 return func(*argslist)
    #             else:
    #                 return func()
    #         except ArgumentError:
    #             print(self.__error("Invalid arguements"))
    #             return self.default(line)
        
    # Tello Control Commands
    def default(self, arg):
        print(self.__error("Unknown Command"))
        
    def do_forward(self, arg):
        try:
            if arg == "":
                self.move_forward(self.precision)
            else:
                    self.move_forward(int(arg))
        except Exception as err:
            print("Failed moving forward", err)
    
    def do_back(self, arg):
        try:
            if arg == "":
                self.move_forward(self.precision)
            else:
                    self.move_forward(int(arg))
        except Exception as err:
            print("Failed moving backward", err)
    
    def do_left(self, arg):
        try:
            if arg == "":
                self.move_forward(self.precision)
            else:
                    self.move_forward(int(arg))
        except Exception as err:
            print("Failed moving left", err)
    
    def do_right(self, arg):
        try:
            if arg == "":
                self.move_forward(self.precision)
            else:
                    self.move_forward(int(arg))
        except Exception as err:
            print("Failed moving right", err)
    
    def do_top(self, arg):
        try:
            if arg == "":
                self.move_forward(self.precision)
            else:
                    self.move_forward(int(arg))
        except Exception as err:
            print("Failed moving top", err)
    
    def do_down(self, arg):
        try:
            if arg == "":
                self.move_forward(self.precision)
            else:
                    self.move_forward(int(arg))
        except Exception as err:
            print("Failed moving down", err)
            
    def do_takeoff(self, arg):
        try:
            self.takeoff()
        except Exception as err:
            print("Failed taking off", err)
            
    def do_land(self, arg):
        try:
            self.land()
        except Exception as err:
            print("Failed landing", err)
            
    def do_rright(self, arg):
        try:
            if arg == "":
                self.rotate_clockwise(self.precision)
            else:
                self.rotate_clockwise(int(arg))
        except Exception as err:
            print("Failed rotating clockwise", err)

    def do_rleft(self, arg):
        try:
            if arg == "":
                self.rotate_counter_clockwise(self.precision)
            else:
                self.rotate_counter_clockwise(int(arg))
        except Exception as err:
            print("Failed rotating counter clockwise", err)
            
    def do_sos(self, arg):
        try:
            self.emergency()
        except Exception as err:
            print("Emergency failed")
            
    def do_streamon(self,arg):
        try:
            self.enable_streaming()
        except Exception as err:
            print("Stream On failed")
            
    def do_streamoff(self,arg):
        try:
            self.disable_streaming()
        except Exception as err:
            print("Stream Off failed")
    
    def do_connect(self,arg):
        try:
            self.connect()
        except Exception as err:
            print("Connection failed")
    
    def do_facetrackeron(self,arg):
        try:
            self.enable_facetracker()
        except Exception as err:
            print("Connection failed")
            
    def do_facetrackeroff(self,arg):
        try:
            self.disable_facetracker()
        except Exception as err:
            print("Connection failed", err)
            
    def do_facefolloweron(self,arg):
        try:
            self.start_following_face()
        except Exception as err:
            print("Connection failed", err)
            
    def do_facefolloweroff(self,arg):
        try:
            self.start_following_face()
        except Exception as err:
            print("Connection failed", err)
            
    def do_gui(self,arg):
        try:
            self.gui()
        except Exception as err:
            print("GUI Failed", err)
            
    def do_automode(self,arg):
        try:
            self.connect()
            self.enable_streaming()
            self.takeoff()
            time.sleep(2)
            self.start_following_face()
            self.gui()
        except Exception as err:
            print("GUI Failed", err)
    
    # def get_names(self):
    #     return ["do_"+control for control in self.tellocontrols] + ["do_bye", "do_exit"]
        
    def do_bye(self, arg):
        'Exit the tello shell'
        return True
    
    do_exit = do_bye
    
if __name__ == "__main__":
    telloconsole = TelloPrompt()
    telloconsole.init_tello()
    telloconsole.cmdloop()