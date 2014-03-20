
__all__ = [
        'Status',
        'Features',
        'ErrorCode',
        ]

class Features():
    WITH_LIBEV  = 1<<0
    WITH_LIBUV  = 1<<1
    WITH_SQLITE = 1<<2
    WITH_MOZJS  = 1<<3

class Status(object):
    LOGOUT  = 0
    ONLINE  = 10
    OFFLINE = 20
    AWAY    = 30
    HIDDEN  = 40
    BUSY    = 50
    CALLME  = 60
    SLIENT  = 70

class ErrorCode(object):
    DB_EXEC_FAILED      = -50
    NOT_JSON_FORMAT     = -30
    #upload error code
    UPLOAD_OVERSIZE     = -21
    UPLOAD_OVERRETRY    = -20
    #network error code
    HTTP_ERROR          = -11
    NETWORK_ERROR       = -10
    #system error code
    FILE_NOT_EXIST      = -6
    NULL_POINTER        = -5
    CANCELED            = -4
    TIMEOUT_OVER        = -3
    NO_RESULT           = -2
    ERROR               = -1

    #webqq error code
    OK                  = 0
    LOGIN_NEED_VC       = 10
    HASH_WRONG          = 50
    LOGIN_ABNORMAL      = 60
    NO_MESSAGE          = 102
    COOKIE_WRONG        = 103
    PTWEBQQ             = 116
    LOST_CONN           = 121

class PollFlags():
    AUTO_DOWN_GROUP_PIC   = 1<<0
    AUTO_DOWN_BUDDY_PIC   = 1<<1
    AUTO_DOWN_DISCU_PIC   = 1<<2
    REMOVE_DUPLICATED_MSG = 1<<3


