/*
 * Mock printf_putchar for system testing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// Mock implementation of _putchar for printf testing
// This just does nothing since we're testing sprintf/snprintf, not printf_
void _putchar(char character) {
    (void)character; // Unused in tests - we test sprintf/snprintf, not printf_
}