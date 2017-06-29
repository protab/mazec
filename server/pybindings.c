#include <Python.h>
#include "pybindings.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "config.h"
#include "draw.h"
#include "event.h"
#include "level.h"
#include "log.h"

struct data_list {
	PyObject *obj;
	struct data_list *next;
};

static PyObject *main_module, *globals;
static PyObject *level_cls;
static PyObject *nope_exc, *win_exc, *lose_exc;

static struct data_list *data_list;
static int data_list_cnt;

#define ERR_MSG_SIZE	128
static char err_msg[ERR_MSG_SIZE];


static wchar_t *to_wchar(const char *src)
{
	wchar_t *dest;
	size_t len;

	len = mbstowcs(NULL, src, 0) + 1;
	dest = salloc(sizeof(wchar_t) * len);
	len = mbstowcs(dest, src, len);
	if (len == (size_t)-1) {
		free(dest);
		return NULL;
	}
	return dest;
}

static char *from_wchar(const wchar_t *src)
{
	char *dest;
	size_t len;

	len = wcstombs(NULL, src, 0) + 1;
	dest = salloc(len);
	len = wcstombs(dest, src, len);
	if (len == (size_t)-1) {
		free(dest);
		return NULL;
	}
	return dest;
}

static char *from_unicode(PyObject *o)
{
	wchar_t *wstr = PyUnicode_AsWideCharString(o, NULL);
	if (!wstr)
		return NULL;
	char *str = from_wchar(wstr);
	PyMem_Free(wstr);
	return str;
}

static void fatal(void)
{
	PyObject *ptype, *pvalue, *ptraceback;
	PyObject *module = NULL, *func = NULL;
	PyObject *exc = NULL;
	Py_ssize_t len;

	log_err("Python exception:");
	PyErr_Fetch(&ptype, &pvalue, &ptraceback);
	PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
	if (!pvalue) {
		pvalue = Py_None;
		Py_INCREF(pvalue);
	}
	if (!ptraceback) {
		ptraceback = Py_None;
		Py_INCREF(ptraceback);
	}
	module = PyImport_ImportModule("traceback");
	if (!module)
		goto error;
	func = PyObject_GetAttrString(module, "format_exception");
	if (!func || !PyCallable_Check(func))
		goto error;
	exc = PyObject_CallFunctionObjArgs(func, ptype, pvalue, ptraceback, NULL);
	if (!exc)
		goto error;
	len = PySequence_Length(exc);
	for (Py_ssize_t i = 0; i < len; i++) {
		PyObject *item = PySequence_GetItem(exc, i);
		if (!item)
			goto error;
		char *str = from_unicode(item);

		log_err("%s", str);

		sfree(str);
		Py_DECREF(item);
	}
	goto out;

error:
	log_err("python: exception handling error");
out:
	Py_XDECREF(exc);
	Py_XDECREF(func);
	Py_XDECREF(module);
	Py_XDECREF(ptype);
	Py_XDECREF(pvalue);
	Py_XDECREF(ptraceback);
	exit(1);
}

static PyObject *c(PyObject *o)
{
	if (!o)
		fatal();
	return o;
}

static void cz(int res)
{
	if (res < 0)
		fatal();
}

static char *to_err_msg(PyObject *o)
{
	PyObject *s;
	wchar_t *wstr;
	size_t len;

	s = c(PyObject_Str(o));
	wstr = PyUnicode_AsWideCharString(s, NULL);
	if (!wstr) {
		Py_DECREF(s);
		return NULL;
	}
	len = wcstombs(err_msg, wstr, ERR_MSG_SIZE);
	if (len == ERR_MSG_SIZE)
		err_msg[ERR_MSG_SIZE - 1] = '\0';
	PyMem_Free(wstr);
	Py_DECREF(s);
	if (len == (size_t)-1)
		strlcpy(err_msg, "???", ERR_MSG_SIZE);

	return err_msg;
}

static char *exc_err(void)
{
	PyObject *ptype, *pvalue, *ptraceback;
	char *res;

	PyErr_Fetch(&ptype, &pvalue, &ptraceback);
	res = to_err_msg(pvalue);
	Py_XDECREF(ptype);
	Py_XDECREF(pvalue);
	Py_XDECREF(ptraceback);
	return res;
}

static bool nope_check(PyObject *o)
{
	if (o)
		return false;
	if (PyErr_ExceptionMatches(nope_exc))
		return true;
	fatal();
	return false; /* silence gcc */
}

static long to_long(PyObject *o)
{
	long res = PyLong_AsLong(o);
	if (PyErr_Occurred())
		fatal();
	return res;
}

/* the timer class */

typedef struct {
	PyObject_HEAD
	int fd;
} timer_object_t;

static int cb_timer(int fd __unused, int count, void *data)
{
	PyObject *self = data;

	Py_DECREF(c(PyObject_CallMethod(self, "fired", "i", count)));
	return 0;
}

static void f_timer_dealloc(PyObject *selfobj)
{
	timer_object_t *self = (timer_object_t *)selfobj;

	if (self->fd >= 0)
		timer_del(self->fd);
	/* It's guaranteed that after timer_del, the callback won't be
	 * called. We may thus safely free our data here. */
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *f_timer_new(PyTypeObject *type, PyObject *args __unused,
			     PyObject *kwds __unused)
{
	timer_object_t *self = (timer_object_t *)type->tp_alloc(type, 0);
	if (self) {
		self->fd = timer_new(cb_timer, self, NULL);
		if (self->fd < 0) {
			PyErr_SetString(PyExc_OSError, "cannot allocate timer");
			Py_DECREF(self);
			return NULL;
		}
	}
	return (PyObject *)self;
}

static PyObject *f_timer_arm(PyObject *selfobj, PyObject *args)
{
	timer_object_t *self = (timer_object_t *)selfobj;
	int milisecs;
	int repeat = 0;

	if (!PyArg_ParseTuple(args, "i|p:arm", &milisecs, &repeat))
		return NULL;
	log_info("fd %d, milisecs %d, repeat %d", self->fd, milisecs, repeat);
	if (timer_arm(self->fd, milisecs, repeat) < 0) {
		PyErr_SetString(PyExc_OSError, "cannot arm timer");
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *f_timer_disarm(PyObject *selfobj, PyObject *args __unused)
{
	timer_object_t *self = (timer_object_t *)selfobj;

	if (timer_disarm(self->fd) < 0) {
		PyErr_SetString(PyExc_OSError, "cannot disarm timer");
		return NULL;
	}
	Py_RETURN_NONE;
}

static PyObject *f_timer_fired(PyObject *selfobj __unused, PyObject *args __unused)
{
	PyErr_SetString(PyExc_AssertionError, "please subclass Timer and redefine the fired method");
	return NULL;
}

static PyMethodDef timer_methods[] = {
	{ "arm", f_timer_arm, METH_VARARGS,
	  "arm(milisecs, repeat=False)\n--\n\n"
	  "Arms (sets) the given timer. It will fire after the given number of miliseconds.\n"
	  "If repeat is False, it will be a one shot (but it's of course possible to call\n"
	  "the arm method again), if it's True, the timer will fire repeatedly every\n"
	  "milisecs. Each time the timer fires, the fired method is called." },
	{ "disarm", f_timer_disarm, METH_NOARGS,
	  "disarm()\n--\n\n"
	  "Disarms the given timer. It's guaranteed that the fired method won't be called\n"
	  "after this call." },
	{ "fired", f_timer_fired, METH_VARARGS,
	  "fired(count)\n--\n\n"
	  "Called upon timer firing. The count parameter contains a value indicating how\n"
	  "many times the alarm was fired since the last call. This may be greater than one\n"
	  "in case of missing a repeated timer." },
	{ NULL, NULL, 0, NULL }
};

static PyTypeObject timer_type = {
	.ob_base = PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "level.Timer",
	.tp_basicsize = sizeof(timer_object_t),
	.tp_dealloc = f_timer_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_doc = "Class for one shot and periodic timers. Create your own subclass and redefine\nthe fired method.",
	.tp_methods = timer_methods,
	.tp_new = f_timer_new,
};

/* drawing functions exported to Python */

static PyObject *f_draw_dirty(PyObject *self __unused, PyObject *args __unused)
{
	level_dirty();
	Py_RETURN_NONE;
}

static PyObject *f_draw_clear(PyObject *self __unused, PyObject *args __unused)
{
	draw_clear();
	Py_RETURN_NONE;
}

static PyObject *f_draw_origin(PyObject *self __unused, PyObject *args)
{
	int x, y;

	if (!PyArg_ParseTuple(args, "ii:origin", &x, &y))
		return NULL;
	draw_set_origin(x, y);
	Py_RETURN_NONE;
}

static PyObject *f_draw_item(PyObject *self __unused, PyObject *args)
{
	int x, y;
	unsigned angle, color;

	if (!PyArg_ParseTuple(args, "iiII:item", &x, &y, &angle, &color))
		return NULL;
	draw_item(x, y, angle, color);
	Py_RETURN_NONE;
}

/* the draw module definition */

static PyMethodDef draw_methods[] = {
	{ "dirty", f_draw_dirty, METH_NOARGS,
	  "dirty()\n--\n\n"
	  "Marks the remote screen as dirty. Call whenever there's a change to the display.\n"
	  "If the level is large and there are off screen changes, it is advisable not to\n"
	  "call this function when the change is not visible." },
	{ "clear", f_draw_clear, METH_NOARGS,
	  "clear()\n--\n\n"
	  "Clears the whole screen." },
	{ "origin", f_draw_origin, METH_VARARGS,
	  "origin(x, y)\n--\n\n"
	  "Sets the upper left corner of the visible area to x, y. Note that if you move\n"
	  "the origin over a block boundary, you'll have to redraw all items. If in doubt,\n"
	  "always redraw after changing the origin." },
	{ "item", f_draw_item, METH_VARARGS,
	  "item(x, y, angle, color)\n--\n\n"
	  "Draw an item to the x, y coordinates under the given angle.\n"
	  "It's okay to pass items that are off screen to this function. However, if it's\n"
	  "cheap not to do that, it's of course more efficient. If you need to check coords\n"
	  "of each item individually to determine whether it's off screen, do not bother\n"
	  "and just call this function, it will do that for you." },
	{ NULL, NULL, 0, NULL }
};

static PyModuleDef draw_module = {
	PyModuleDef_HEAD_INIT, "draw",
	"Level drawing functions.",
	-1, draw_methods, NULL, NULL, NULL, NULL
};

/* level functions exported to Python */

static PyObject *f_level_set_level(PyObject *self __unused, PyObject *args)
{
	PyObject *code;

	if (!PyArg_ParseTuple(args, "OO:set_level", &code, &level_cls))
		return NULL;
	Py_INCREF(level_cls);
	Py_RETURN_NONE;
}

/* the level module definition */

static PyMethodDef level_methods[] = {
	{ "set_level", f_level_set_level, METH_VARARGS,
	  "set_level(code, klass)\n--\n\n"
	  "Registers a level class. The first argument is a level code, the second argument\n"
	  "is a sublass of BaseLevel." },
	{ NULL, NULL, 0, NULL }
};

static PyModuleDef level_module = {
	PyModuleDef_HEAD_INIT, "level",
	"Level handling and drawing functions.",
	-1, level_methods, NULL, NULL, NULL, NULL
};

static PyObject *init_level_module(void)
{
	PyObject *lm, *dm;

	dm = c(PyModule_Create(&draw_module));

	cz(PyModule_AddIntConstant(dm, "MOD", DRAW_MOD));
	cz(PyModule_AddIntConstant(dm, "WIDTH", DRAW_WIDTH));
	cz(PyModule_AddIntConstant(dm, "HEIGHT", DRAW_HEIGHT));
	cz(PyModule_AddIntConstant(dm, "MOD_WIDTH", DRAW_MOD_WIDTH));
	cz(PyModule_AddIntConstant(dm, "MOD_HEIGHT", DRAW_MOD_HEIGHT));

	lm = c(PyModule_Create(&level_module));

	nope_exc = c(PyErr_NewExceptionWithDoc("level.Nope",
		"Raised when a given command is not supported by the level.",
		NULL, NULL));
	win_exc = c(PyErr_NewExceptionWithDoc("level.Win",
		"Raised when the player won. Can be only raised from the move method.",
		NULL, NULL));
	lose_exc = c(PyErr_NewExceptionWithDoc("level.Lose",
		"Raised when the player lost. Can be only raised from the move method.",
		NULL, NULL));

	cz(PyModule_AddObject(lm, "Nope", nope_exc));
	cz(PyModule_AddObject(lm, "Win", win_exc));
	cz(PyModule_AddObject(lm, "Lose", lose_exc));

	cz(PyType_Ready(&timer_type));
	Py_INCREF(&timer_type);
	cz(PyModule_AddObject(lm, "Timer", (PyObject *)&timer_type));

	cz(PyModule_AddObject(lm, "draw", dm));

	return lm;
}

/* python callback interface */

void *pyb_get_data(void)
{
	struct data_list **last, *d;

	PyObject *o = c(PyObject_CallFunctionObjArgs(level_cls, NULL));

	for (last = &data_list; *last; last = &(*last)->next)
		;
	d = salloc(sizeof(*d));
	d->obj = o;
	d->next = NULL;
	*last = d;
	data_list_cnt++;

	return d;
}

int pyb_move(void *data, char c, char **msg)
{
	struct data_list *d = data;
	char buf[2];

	buf[0] = c;
	buf[1] = '\0';
	PyObject *o = PyObject_CallMethod(d->obj, "move", "(s)", buf);
	if (!o) {
		if (PyErr_ExceptionMatches(nope_exc)) {
			*msg = exc_err();
			return MOVE_BAD;
		}
		if (PyErr_ExceptionMatches(win_exc)) {
			*msg = exc_err();
			return MOVE_WIN;
		}
		if (PyErr_ExceptionMatches(lose_exc)) {
			*msg = exc_err();
			return MOVE_LOSE;
		}
		fatal();
	}
	Py_DECREF(o);
	return MOVE_OKAY;
}

char *pyb_what(void *data, int x, int y, int *res)
{
	struct data_list *d = data;

	PyObject *o = PyObject_CallMethod(d->obj, "what", "ii", x, y);
	if (nope_check(o))
		return exc_err();
	*res = to_long(o);
	Py_DECREF(o);
	return NULL;
}

char *pyb_maze(void *data, unsigned char **res, unsigned *len)
{
	struct data_list *d = data;
	Py_ssize_t seqlen;

	PyObject *seqret = PyObject_CallMethod(d->obj, "maze", NULL);
	if (nope_check(seqret))
		return exc_err();
	PyObject *seq = c(PySequence_Fast(seqret, "the maze method must return a sequence"));
	seqlen = PySequence_Fast_GET_SIZE(seq);
	*len = seqlen;
	*res = salloc(seqlen);
	for (Py_ssize_t i = 0; i < seqlen; i++) {
		PyObject *o = PySequence_Fast_GET_ITEM(seq, i);
		(*res)[i] = to_long(o);
	}
	Py_DECREF(seq);
	Py_DECREF(seqret);
	return NULL;
}

char *pyb_get(void *data, const char *attr, int *res)
{
	struct data_list *d = data;

	PyObject *o = PyObject_GetAttrString(d->obj, attr);
	if (nope_check(o))
		return exc_err();
	*res = to_long(o);
	Py_DECREF(o);
	return NULL;
}

char *pyb_get_x(void *data, int *res)
{
	return pyb_get(data, "x", res);
}

char *pyb_get_y(void *data, int *res)
{
	return pyb_get(data, "y", res);
}

char *pyb_get_w(void *data, int *res)
{
	return pyb_get(data, "w", res);
}

char *pyb_get_h(void *data, int *res)
{
	return pyb_get(data, "h", res);
}

void pyb_redraw(void)
{
	struct data_list *d = data_list;
	PyObject *objs = c(PyTuple_New(data_list_cnt));

	for (int i = 0; i < data_list_cnt; i++) {
		Py_INCREF(d->obj);
		PyTuple_SET_ITEM(objs, i, d->obj);
		d = d->next;
	}
	PyObject *method = c(PyObject_GetAttrString(level_cls, "redraw"));
	Py_DECREF(c(PyObject_CallFunction(method, "OO", level_cls, objs)));
	Py_DECREF(method);
	Py_DECREF(objs);
}

/* python level interface */

static struct level_ops ops = {
	.get_data = pyb_get_data,
	.move = pyb_move,
	.what = pyb_what,
	.maze = pyb_maze,
	.get_x = pyb_get_x,
	.get_y = pyb_get_y,
	.get_w = pyb_get_w,
	.get_h = pyb_get_h,
	.redraw = pyb_redraw,
};

static void set_level_parms(void)
{
	PyObject *max_conn = c(PyObject_GetAttrString(level_cls, "max_conn"));
	PyObject *max_time = c(PyObject_GetAttrString(level_cls, "max_time"));
	ops.max_conn = to_long(max_conn);
	ops.max_time = to_long(max_time);
	Py_DECREF(max_time);
	Py_DECREF(max_conn);
}

static bool pyb_init()
{

	data_list = NULL;
	data_list_cnt = 0;

	level_cls = NULL;

	if (PyImport_AppendInittab("level", init_level_module) < 0) {
		log_err("python: cannot register the level module");
		return false;
	}

	Py_Initialize();

	main_module = c(PyImport_AddModule("__main__"));
	Py_INCREF(main_module);
	globals = PyModule_GetDict(main_module);

	PyObject *path = PySys_GetObject("path");
	if (!path) {
		log_err("python: cannot get sys.path");
		exit(1);
	}
	PyObject *p = c(PyUnicode_FromString(PYLEVELS_DIR));
	cz(PyList_Append(path, p));
	Py_DECREF(p);

	return true;
}

struct level_ops *pyb_load(const char *path)
{
	FILE *f;

	f = fopen(path, "r");
	if (!f) {
		log_err("cannot open %s", path);
		return NULL;
	}

	log_info("python: importing %s", path);
	Py_SetProgramName(to_wchar(path));
	if (!pyb_init(path))
		return NULL;

	PyObject *o = c(PyRun_FileEx(f, path, Py_file_input, globals, globals, true));
	Py_DECREF(o);

	if (!level_cls) {
		log_err("python: the Python level did not call set_level");
		exit(1);
	}
	set_level_parms();

	return &ops;
}

void pyb_interactive(void)
{
	if (!pyb_init())
		return;
	PyImport_ImportModuleEx("readline", globals, globals, NULL);
	if (PyRun_InteractiveLoop(stdin, "python") < 0)
		fatal();
	return;
}
