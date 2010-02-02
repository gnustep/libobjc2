
void GCPerformDeferred(void(*function)(void*), void *data, 
		int useconds);
void GCPerform(void(*function)(void*), void *data);
