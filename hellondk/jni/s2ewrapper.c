#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <android/log.h>
#include <s2earm.h>

jint Java_ch_epfl_s2e_android_S2EAndroidWrapper_getVersion(JNIEnv * env, jobject this)
{
	return s2e_version();
}

void Java_ch_epfl_s2e_android_S2EAndroidWrapper_printMessage(JNIEnv * env, jobject this, jstring message)
{
	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, message, &isCopy);
	s2e_message(szMsg);
}

void Java_ch_epfl_s2e_android_S2EAndroidWrapper_printExpressionInt(JNIEnv * env, jobject this, jint expression, jstring name)
{
	jboolean isCopy;
	const char * szName = (*env)->GetStringUTFChars(env, name, &isCopy);
	s2e_print_expression(expression, name);
}

void Java_ch_epfl_s2e_android_S2EAndroidWrapper_printWarning(JNIEnv * env, jobject this, jstring message)
{
	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, message, &isCopy);
	s2e_warning(szMsg);
}

void Java_ch_epfl_s2e_android_S2EAndroidWrapper_enableForking(JNIEnv * env, jobject this) {
	s2e_enable_forking();
}

void Java_ch_epfl_s2e_android_S2EAndroidWrapper_disableForking(JNIEnv * env, jobject this) {
	s2e_disable_forking();
}

void Java_ch_epfl_s2e_android_S2EAndroidWrapper_disableInterrupts(JNIEnv * env, jobject this) {
	s2e_disable_interrupts();
}

void Java_ch_epfl_s2e_android_S2EAndroidWrapper_enableInterrupts(JNIEnv * env, jobject this) {
	s2e_enable_interrupts();
}

void Java_ch_epfl_s2e_android_S2EAndroidWrapper_killState(JNIEnv * env, jobject this, jint status, jstring msg) {

	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, msg, &isCopy);
	s2e_kill_state(status,szMsg);
}

jint Java_ch_epfl_s2e_android_S2EAndroidWrapper_getSymbolicInt(JNIEnv * env, jobject this,jstring name) {
	jboolean iscopy;
	int x;
	const char *symbol = (*env)->GetStringUTFChars(
                env, name, &iscopy);
    s2e_make_symbolic(&x, sizeof(x), symbol);
    return x;
}

jdouble Java_ch_epfl_s2e_android_S2EAndroidWrapper_getSymbolicDouble(JNIEnv * env, jobject this,jstring name) {
	jboolean iscopy;
	double x;
	const char *symbol = (*env)->GetStringUTFChars(
                env, name, &iscopy);
    s2e_make_symbolic(&x, sizeof(x), symbol);
    return x;
}

jlong Java_ch_epfl_s2e_android_S2EAndroidWrapper_getSymbolicLong(JNIEnv * env, jobject this,jstring name) {
	jboolean iscopy;
	long x;
	const char *symbol = (*env)->GetStringUTFChars(
                env, name, &iscopy);
    s2e_make_symbolic(&x, sizeof(x), symbol);
    return x;
}

jfloat Java_ch_epfl_s2e_android_S2EAndroidWrapper_getSymbolicFloat(JNIEnv * env, jobject this,jstring name) {
	jboolean iscopy;
	float x;
	const char *symbol = (*env)->GetStringUTFChars(
                env, name, &iscopy);
    s2e_make_symbolic(&x, sizeof(x), symbol);
    return x;
}

jboolean Java_ch_epfl_s2e_android_S2EAndroidWrapper_getSymbolicBoolean(JNIEnv * env, jobject this,jstring name) {
	jboolean iscopy;
	int x;
	const char *symbol = (*env)->GetStringUTFChars(
                env, name, &iscopy);
    s2e_make_symbolic(&x, sizeof(x), symbol);
    //TODO: jboolean is represented as 8-bit unsigned C type that can store values from 0 to 255.
    //      The value 0 corresponds to the constant JNI_FALSE, and the values from 1 to 255 correspond to JNI_TRUE
    return (jboolean)x;
}

jint Java_ch_epfl_s2e_android_S2EAndroidWrapper_getExampleInt(JNIEnv * env, jobject this,jint var) {
	s2e_get_example(&var, sizeof(var));
	return var;
}

jdouble Java_ch_epfl_s2e_android_S2EAndroidWrapper_getExampleDouble(JNIEnv * env, jobject this,jdouble var) {
	s2e_get_example(&var, sizeof(var));
	return var;
}

jlong Java_ch_epfl_s2e_android_S2EAndroidWrapper_getExampleLong(JNIEnv * env, jobject this,jlong var) {
	s2e_get_example(&var, sizeof(var));
	return var;
}

jfloat Java_ch_epfl_s2e_android_S2EAndroidWrapper_getExampleFloat(JNIEnv * env, jobject this,jfloat var) {
	s2e_get_example(&var, sizeof(var));
	return var;
}

jboolean Java_ch_epfl_s2e_android_S2EAndroidWrapper_getExampleBoolean(JNIEnv * env, jobject this,jboolean var) {
	s2e_get_example(&var, sizeof(var));
	return var;
}

jint Java_ch_epfl_s2e_android_S2EAndroidWrapper_concretizeInt(JNIEnv * env, jobject this,jint var) {
	s2e_concretize(&var,sizeof(var));
	return var;
}

jdouble Java_ch_epfl_s2e_android_S2EAndroidWrapper_concretizeDouble(JNIEnv * env, jobject this,jdouble var) {
	s2e_concretize(&var,sizeof(var));
	return var;
}

jlong Java_ch_epfl_s2e_android_S2EAndroidWrapper_concretizeLong(JNIEnv * env, jobject this,jlong var) {
	s2e_concretize(&var,sizeof(var));
	return var;
}

jfloat Java_ch_epfl_s2e_android_S2EAndroidWrapper_concretizeFloat(JNIEnv * env, jobject this,jfloat var) {
	s2e_concretize(&var,sizeof(var));
	return var;
}

jboolean Java_ch_epfl_s2e_android_S2EAndroidWrapper_concretizeBoolean(JNIEnv * env, jobject this,jboolean var) {
	s2e_concretize(&var,sizeof(var));
	return var;
}

void Java_ch_epfl_s2e_android_S2EAndroidWrapper_assertThat(JNIEnv * env, jobject this, jboolean condition, jstring failMessage) {
	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, failMessage, &isCopy);
	_s2e_assert(condition,szMsg);
}

void Java_ch_epfl_s2e_android_S2EAndroidWrapper_traceAndroidLocation(JNIEnv * env, jobject this,jstring message) {
	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, message, &isCopy);
	s2e_android_trace_location(szMsg);
}

void Java_ch_epfl_s2e_android_S2EAndroidWrapper_traceAndroidUID(JNIEnv * env, jobject this,jstring message) {
	jboolean isCopy;
	const char * szMsg = (*env)->GetStringUTFChars(env, message, &isCopy);
	s2e_android_trace_uid(szMsg);
}
