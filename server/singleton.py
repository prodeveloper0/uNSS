def singleton(clazz):
    instance = None
    sealed = False

    def class__new__(cls, *args, **kwargs):
        nonlocal instance
        if instance is None:
            instance = clazz.__new__(cls)
            instance._sealed = False
        return instance

    def class___init__(self, *args, **kwargs):
        nonlocal sealed
        if sealed:
            return
        clazz.__init__(self, *args, **kwargs)
        sealed = True

    new_clazz = type(clazz.__name__, (clazz,), {
        '__new__': class__new__,
        '__init__': class___init__,
    })

    return new_clazz
