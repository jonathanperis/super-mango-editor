// Boot the game with --debug flag enabled
Module.onRuntimeInitialized = function() {
    Module.callMain(["--debug"]);
};
