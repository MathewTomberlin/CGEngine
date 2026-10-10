from PyScript import PyScript

# Example script for the control channel's attach_script command.
# On its first call it writes the name of the body it is attached to into cg_control/script_hits.log,
# so an agent can confirm that the hook ran. Later calls do nothing, which keeps the log small.
class ControlHitLogger(PyScript):
    def __init__(self):
        super().__init__()
        self.logged = False

    def __call__(self, args):
        if self.logged or args.caller is None:
            return
        self.logged = True
        with open("cg_control/script_hits.log", "a") as log:
            log.write(args.caller.get_name() + "\n")

def create_instance():
    return ControlHitLogger()
