from PyScript import PyScript
import cg_engine_bindings as cge
from cg_engine_bindings import ScArgs, Script, Body, Behavior, Vector2f, Vector3f # Assuming Vector2f is bound

class DevPyScript(PyScript):
    """
    Example developer-defined PyScript subclass.
    Moves the attached Body based on data stored in the C++ Script object's input.
    """
    def __init__(self, initial_speed=1.0):
        """ Constructor for this specific script logic. """
        super().__init__() # Call the base class constructor is good practice
        self.python_speed_multiplier = float(initial_speed) # Example Python-specific state

    def __call__(self, args: ScArgs):
        """
        Overrides the base class method. This is the core logic executed
        when the C++ Script::call invokes the associated pyCallable.

        Args:
            args (cge.ScArgs): The script arguments passed from C++.
        """
        try:
            #TODO: Do we really need to declare one method for each data type?
            base_speed = args.script.get_input_data_float("speed")
            move_x = args.script.get_input_data_float("move_x")
            move_y = args.script.get_input_data_float("move_y")
            #TODO: DeltaTime from script data
        except Exception as e:
            print(f"Error getting input data from args.script in DevPyScript: {e}")
            base_speed, move_x, move_y = 0.0, 0.0, 0.0 # Defaults

        # Calculate movement using Python state and C++ state
        delta_time = 0.016
        final_speed = base_speed * self.python_speed_multiplier
        move_vec = Vector3f(move_x * final_speed * delta_time, move_y * final_speed * delta_time, 0)

        # Interact with the C++ Body (requires Body methods like move, get_name to be bound)
        try:
            if move_vec.x != 0 or move_vec.y != 0:
                args.caller.get_mesh().move(move_vec)
        except AttributeError as e:
            print(f"[PyScript] ERROR: Potentially unbound method on args.caller (Body) in DevPyScript: {e}")
        except Exception as e:
            print(f"[PyScript] ERROR: Body interaction in DevPyScript failed: {e}")

        # Set output data on the C++ Script object
        try:
            args.script.set_output_data_string("status", "Moved this frame")
        except Exception as e:
            print(f"Error setting output data on args.script in DevPyScript: {e}")

# --- Factory Function ---
# This function MUST exist at the module level with this exact name.
def create_instance():
    """ Engine calls this function via C++ to get an instance of the script logic. """
    return DevPyScript()