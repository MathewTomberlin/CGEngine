import cg_engine_bindings as cge
from cg_engine_bindings import ScArgs, Script, Body, Behavior, Vector2f

class PyScript:
    """
    Engine base class for developer-defined Python ScriptEvent scripts.
    Developers should inherit from this class and override the '__call__' method
    to implement their desired logic, which will be executed when the
    corresponding C++ Script object is called by the engine.
    """
    def __init__(self, **kwargs):
        """
        Base class constructor. Can accept arbitrary keyword arguments
        passed from the C++ attach function if needed, but doesn't
        use them by default.
        """
        pass

    def __call__(self, args: ScArgs):
        """
        The main execution method called by the C++ Script object.
        *** Derived classes MUST override this method. ***

        Args:
            args (cge.ScArgs): The script arguments passed from C++.
                               Provides access to .script, .caller, .behavior
        """
        raise NotImplementedError("Derived class must implement __call__ method")