typedef unsigned char u8;
typedef int s32;
extern s32 func_0005c110(s32);

/* Numeric byte selectors and query IDs follow the complete original body.
 * Each nested switch retains its own query sites; the two published bytes
 * and their meanings are deliberately unnamed. */
void func_0013A558(u8 input0, u8 input1, u8 *first, u8 *second) {
    u8 result0 = 0;
    u8 result1 = 0;

    if (((u8)input0 == 0) | ((u8)input0 == 16)) {
        input0 = 9;
    }
    /* Keep the switch arms in the original destination order. Equal output
     * pairs assign result1 first; this preserves the native shared suffixes. */
    switch ((u8)input0) {
    case 1:
        switch ((u8)input1) {
        case 1:
        case 2:
            result1 = 1;
            result0 = 1;
            break;
        case 3:
            if (func_0005c110(1) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 4:
        case 5:
        case 7:
            if (func_0005c110(1) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
        case 8:
        case 9:
            if (func_0005c110(1) != 0) {
                if (func_0005c110(2) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    if (func_0005c110(3) != 0) {
                        result0 = 0;
                        result1 = 1;
                    } else {
                        result1 = 0;
                        result0 = 0;
                    }
                }
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 2:
    case 10:
        switch ((u8)input1) {
        case 1:
        case 2:
            result1 = 1;
            result0 = 1;
            break;
        case 3:
        case 4:
        case 5:
        case 7:
            if (func_0005c110(1) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
        case 8:
        case 9:
            if (func_0005c110(1) != 0) {
                if (func_0005c110(2) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    if (func_0005c110(3) != 0) {
                        result0 = 0;
                        result1 = 1;
                    } else {
                        result1 = 0;
                        result0 = 0;
                    }
                }
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 3:
        switch ((u8)input1) {
        case 1:
        case 2:
            if (func_0005c110(1) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
            result1 = 1;
            result0 = 1;
            break;
        case 6:
            if (func_0005c110(2) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 8:
            if (func_0005c110(3) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(2) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 9:
            if (func_0005c110(2) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 4:
        switch ((u8)input1) {
        case 1:
            if (func_0005c110(1) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 2:
            if (func_0005c110(1) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
            result1 = 1;
            result0 = 1;
            break;
        case 6:
            if (func_0005c110(2) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 8:
            if (func_0005c110(3) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(2) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 9:
            if (func_0005c110(2) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 5:
    case 11:
        switch ((u8)input1) {
        case 1:
            if (func_0005c110(1) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 2:
            if (func_0005c110(1) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
            result1 = 1;
            result0 = 1;
            break;
        case 6:
            if (func_0005c110(2) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 8:
            if (func_0005c110(2) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        case 9:
            if (func_0005c110(2) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        }
        break;
    case 6:
        switch ((u8)input1) {
        case 1:
        case 2:
            if (func_0005c110(1) != 0) {
                if (func_0005c110(2) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    if (func_0005c110(3) != 0) {
                        result0 = 0;
                        result1 = 1;
                    } else {
                        result1 = 0;
                        result0 = 0;
                    }
                }
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
            if (func_0005c110(2) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 6:
        case 8:
        case 9:
            result1 = 1;
            result0 = 1;
            break;
        case 7:
            if (func_0005c110(2) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 7:
    case 12:
        switch ((u8)input1) {
        case 1:
            if (func_0005c110(1) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 2:
            if (func_0005c110(1) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
            result1 = 1;
            result0 = 1;
            break;
        case 8:
            if (func_0005c110(3) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(2) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 6:
            if (func_0005c110(2) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 9:
            if (func_0005c110(3) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(2) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 8:
        switch ((u8)input1) {
        case 1:
        case 2:
            if (func_0005c110(1) != 0) {
                if (func_0005c110(2) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    if (func_0005c110(3) != 0) {
                        result0 = 0;
                        result1 = 1;
                    } else {
                        result1 = 0;
                        result0 = 0;
                    }
                }
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 7:
            if (func_0005c110(3) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(2) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 5:
            if (func_0005c110(2) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 6:
        case 8:
        case 9:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 9:
        switch ((u8)input1) {
        case 1:
        case 2:
            if (func_0005c110(1) != 0) {
                if (func_0005c110(2) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    if (func_0005c110(3) != 0) {
                        result0 = 0;
                        result1 = 1;
                    } else {
                        result1 = 0;
                        result0 = 0;
                    }
                }
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
            if (func_0005c110(2) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 5:
            if (func_0005c110(2) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(3) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    
                }
            }
            break;
        case 7:
            if (func_0005c110(3) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(2) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    
                }
            }
            break;
        case 6:
        case 8:
        case 9:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    default:
        result1 = 0;
        result0 = 0;
        break;
    }
    *first = result0;
    *second = result1;
}



extern s32 func_0005c110(s32);

/* Numeric selectors and ordered flag reads are reconstructed from the
 * complete original body. Repeated queries are intentional. */
void func_0013AA40(u8 input0, u8 input1, u8 *first, u8 *second) {
    u8 result0 = 0;
    u8 result1 = 0;

    if ((u8)input0 == 0) input0 = 18;
    /* Keep the switch arms in the original destination order. Equal output
     * pairs assign result1 first; this preserves the native shared suffixes. */
    switch ((u8)input0) {
    case 1:
    case 2:
    case 19:
        switch ((u8)input1) {
        case 3:
            if (func_0005c110(4) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 4:
        case 5:
        case 7:
        case 9:
            if (func_0005c110(4) != 0) {
                result0 = (u8)input1 == 2;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
        case 8:
        case 10:
        case 11:
        case 12:
        case 14:
        case 16:
            if (func_0005c110(4) != 0 && (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 13:
        case 15:
        case 17:
        case 18:
            if (func_0005c110(4) != 0 && (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) && (func_0005c110(8) != 0 || func_0005c110(9) != 0 || func_0005c110(10) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 1:
        case 2:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 3:
        switch ((u8)input1) {
        case 1:
        case 2:
            if (func_0005c110(4) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 11:
        case 12:
        case 14:
        case 16:
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
            if (func_0005c110(5) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 8:
            if (func_0005c110(6) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(5) != 0 || func_0005c110(7) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 10:
            if (func_0005c110(7) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(5) != 0 || func_0005c110(6) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 13:
        case 15:
        case 17:
        case 18:
            if ((func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) && (func_0005c110(8) != 0 || func_0005c110(9) != 0 || func_0005c110(10) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
        case 9:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 4:
        switch ((u8)input1) {
        case 11:
        case 12:
        case 14:
        case 16:
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 1:
        case 2:
            if (func_0005c110(4) != 0) {
                result0 = (u8)input1 == 2;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
            if (func_0005c110(5) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 8:
            if (func_0005c110(6) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(5) != 0 || func_0005c110(7) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 10:
            if (func_0005c110(7) != 0) {
                result1 = 1;
                result0 = 1;
            } else {
                if (func_0005c110(5) != 0 || func_0005c110(6) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 13:
        case 15:
        case 17:
        case 18:
            if ((func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) && (func_0005c110(8) != 0 || func_0005c110(9) != 0 || func_0005c110(10) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
        case 9:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 5:
    case 7:
    case 9:
        switch ((u8)input1) {
        case 1:
        case 2:
            if (func_0005c110(4) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
            if ((u8)input0 == 5 && func_0005c110(5) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 8:
            if ((u8)input0 == 7 && func_0005c110(6) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 10:
            if ((u8)input0 == 9 && func_0005c110(7) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 11:
        case 12:
        case 14:
        case 16:
            {
                /* Materialize the narrowed selector once at word width. Keeping it
                 * as a byte repeats the masks and changes the native body. */
                s32 selected = (u8)input0;
                if (selected == 5 && func_0005c110(5) != 0) {
                    result1 = 1;
                    result0 = 1;
                    break;
                }
                if (selected == 7 && func_0005c110(6) != 0) {
                    result1 = 1;
                    result0 = 1;
                    break;
                }
                if (selected == 9 && func_0005c110(7) != 0) {
                    result1 = 1;
                    result0 = 1;
                    break;
                }
                if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 13:
        case 15:
        case 17:
        case 18:
            if ((func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) && (func_0005c110(8) != 0 || func_0005c110(9) != 0 || func_0005c110(10) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
        case 9:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 20:
    case 21:
    case 22:
        switch ((u8)input1) {
        case 1:
        case 2:
            if (func_0005c110(4) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
            if ((u8)input0 == 20 && func_0005c110(5) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 8:
            if ((u8)input0 == 21 && func_0005c110(6) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 10:
            if ((u8)input0 == 22 && func_0005c110(7) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 11:
        case 12:
        case 14:
        case 16:
            {
                /* Materialize the narrowed selector once at word width. Keeping it
                 * as a byte repeats the masks and changes the native body. */
                s32 selected = (u8)input0;
                if (selected == 20 && func_0005c110(5) != 0) {
                    result1 = 1;
                    result0 = 1;
                    break;
                }
                if (selected == 21 && func_0005c110(6) != 0) {
                    result1 = 1;
                    result0 = 1;
                    break;
                }
                if (selected == 22 && func_0005c110(7) != 0) {
                    result1 = 1;
                    result0 = 1;
                    break;
                }
                if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                    result0 = 0;
                    result1 = 1;
                } else {
                    result1 = 0;
                    result0 = 0;
                }
            }
            break;
        case 13:
        case 15:
        case 17:
        case 18:
            if ((func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) && (func_0005c110(8) != 0 || func_0005c110(9) != 0 || func_0005c110(10) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
        case 9:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 6:
    case 8:
    case 10:
        switch ((u8)input1) {
        case 1:
        case 2:
            if (func_0005c110(4) != 0 && (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
        case 9:
            if ((u8)input0 == 6 && func_0005c110(5) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input0 == 8 && func_0005c110(6) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input0 == 10 && func_0005c110(7) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 13:
        case 15:
        case 17:
        case 18:
            if (func_0005c110(8) != 0 || func_0005c110(9) != 0 || func_0005c110(10) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
        case 8:
        case 10:
        case 11:
        case 12:
        case 14:
        case 16:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 11:
    case 12:
    case 14:
    case 16:
        switch ((u8)input1) {
        case 1:
        case 2:
            if (func_0005c110(4) != 0 && (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
        case 9:
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 13:
        case 15:
        case 17:
        case 18:
            if ((u8)input0 == 12 && func_0005c110(8) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input0 == 14 && func_0005c110(9) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input0 == 16 && func_0005c110(10) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(8) != 0 || func_0005c110(9) != 0 || func_0005c110(10) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
        case 8:
        case 10:
        case 11:
        case 12:
        case 14:
        case 16:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 23:
    case 24:
    case 25:
        switch ((u8)input1) {
        case 1:
        case 2:
            if (func_0005c110(4) != 0 && (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
        case 9:
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 13:
        case 15:
        case 17:
        case 18:
            if ((u8)input0 == 23 && func_0005c110(8) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input0 == 24 && func_0005c110(9) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input0 == 25 && func_0005c110(10) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(8) != 0 || func_0005c110(9) != 0 || func_0005c110(10) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
        case 8:
        case 10:
        case 11:
        case 12:
        case 14:
        case 16:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 13:
    case 15:
    case 17:
    case 18:
        switch ((u8)input1) {
        case 1:
        case 2:
            if (func_0005c110(4) != 0 && (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 7:
        case 9:
            if (func_0005c110(5) != 0 || func_0005c110(6) != 0 || func_0005c110(7) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
        case 8:
        case 10:
        case 11:
        case 12:
        case 14:
        case 16:
            if ((u8)input0 == 13 && func_0005c110(8) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input0 == 15 && func_0005c110(9) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input0 == 17 && func_0005c110(10) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(8) != 0 || func_0005c110(9) != 0 || func_0005c110(10) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 13:
        case 15:
        case 17:
        case 18:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    default:
        result1 = 0;
        result0 = 0;
        break;
    }
    *first = result0;
    *second = result1;
}



extern s32 func_0005c110(s32);

/* Retain numeric selectors and every repeated flag read. Each switch arm
 * assigns the two published bytes without assuming meanings for them. */
void func_0013B350(u8 input0, u8 input1, u8 *first, u8 *second) {
    u8 result0 = 0;
    u8 result1 = 0;

    if ((u8)input0 == 0) input0 = 9;
    /* Keep the switch arms in the original destination order. Equal output
     * pairs assign result1 first; this preserves the native shared suffixes. */
    switch ((u8)input0) {
    case 1:
    case 2:
    case 4:
        switch ((u8)input1) {
        case 3:
        case 5:
            if ((u8)input1 == 3 && func_0005c110(11) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input1 == 5 && func_0005c110(12) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(11) != 0 || func_0005c110(12) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
        case 7:
            if ((u8)input0 == 2 && func_0005c110(11) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input0 == 4 && func_0005c110(12) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(11) != 0 || func_0005c110(12) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 8:
        case 9:
            if (func_0005c110(13) != 0 && (func_0005c110(11) != 0 || func_0005c110(12) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 1:
        case 2:
        case 4:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 10:
    case 11:
        switch ((u8)input1) {
        case 3:
        case 5:
            if ((u8)input1 == 3 && func_0005c110(11) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input1 == 5 && func_0005c110(12) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(11) != 0 || func_0005c110(12) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 8:
        case 9:
            if (func_0005c110(13) != 0 && (func_0005c110(11) != 0 || func_0005c110(12) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 6:
        case 7:
            if ((u8)input0 == 10 && func_0005c110(11) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input0 == 11 && func_0005c110(12) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(11) != 0 || func_0005c110(12) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 1:
        case 2:
        case 4:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 3:
    case 5:
        switch ((u8)input1) {
        case 1:
        case 2:
        case 4:
            if ((u8)input0 == 3 && func_0005c110(11) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if ((u8)input0 == 5 && func_0005c110(12) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(11) != 0 || func_0005c110(12) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 8:
        case 9:
            if ((u8)input1 == 8 && func_0005c110(13) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(13) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 5:
        case 6:
        case 7:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 6:
    case 7:
    case 12:
        switch ((u8)input1) {
        case 1:
        case 2:
        case 4:
            if (func_0005c110(11) != 0 || func_0005c110(12) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 8:
        case 9:
            if ((((u8)input0 == 7) | ((u8)input0 == 12)) && func_0005c110(13) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(13) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 5:
        case 6:
        case 7:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    case 8:
    case 9:
        switch ((u8)input1) {
        case 1:
        case 2:
        case 4:
            if (func_0005c110(13) != 0 && (func_0005c110(11) != 0 || func_0005c110(12) != 0)) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 3:
        case 5:
        case 6:
        case 7:
            if ((u8)input0 == 8 && func_0005c110(13) != 0) {
                result1 = 1;
                result0 = 1;
                break;
            }
            if (func_0005c110(13) != 0) {
                result0 = 0;
                result1 = 1;
            } else {
                result1 = 0;
                result0 = 0;
            }
            break;
        case 8:
        case 9:
            result1 = 1;
            result0 = 1;
            break;
        default:
            result1 = 0;
            result0 = 0;
            break;
        }
        break;
    default:
        result1 = 0;
        result0 = 0;
        break;
    }
    *first = result0;
    *second = result1;
}
