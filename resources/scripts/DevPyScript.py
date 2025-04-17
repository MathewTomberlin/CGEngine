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

    def __call__(self, args: ScArgs):
        """
        Overrides the base class method. This is the core logic executed
        when the C++ Script::call invokes the associated pyCallable.

        Args:
            args (cge.ScArgs): The script arguments passed from C++.
        """

# --- Factory Function ---
# This function MUST exist at the module level with this exact name.
def create_instance():
    """ Engine calls this function via C++ to get an instance of the script logic. """
    return DevPyScript()