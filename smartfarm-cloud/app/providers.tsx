'use client';

import type { ReactNode } from 'react';
import { Amplify } from 'aws-amplify';
import { Authenticator } from '@aws-amplify/ui-react';
import '@aws-amplify/ui-react/styles.css';
import outputs from '../amplify_outputs.json';

Amplify.configure(outputs);

// Wrapping the whole app in <Authenticator> (rather than gating just the
// dashboard page) means no route can ever render app content pre-login —
// the mandatory-auth requirement from the plan, enforced structurally.
export function Providers({ children }: { children: ReactNode }) {
  return <Authenticator>{children}</Authenticator>;
}
