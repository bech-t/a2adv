import { Component, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { MatButtonModule } from '@angular/material/button';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import {
  MAT_DIALOG_DATA,
  MatDialogModule,
  MatDialogRef,
} from '@angular/material/dialog';

export interface PromptDialogData {
  title: string;
  label: string;
  initial?: string;
}

@Component({
  selector: 'app-prompt-dialog',
  imports: [FormsModule, MatButtonModule, MatFormFieldModule, MatInputModule, MatDialogModule],
  template: `
    <h2 mat-dialog-title>{{ data.title }}</h2>
    <mat-dialog-content>
      <mat-form-field appearance="outline" class="full" subscriptSizing="dynamic">
        <mat-label>{{ data.label }}</mat-label>
        <input matInput [(ngModel)]="value" (keydown.enter)="ref.close(value())" />
      </mat-form-field>
    </mat-dialog-content>
    <mat-dialog-actions align="end">
      <button mat-button [mat-dialog-close]="null">Annuler</button>
      <button mat-flat-button color="primary" [mat-dialog-close]="value()">OK</button>
    </mat-dialog-actions>
  `,
  styles: [
    `
      .full {
        width: 320px;
      }
    `,
  ],
})
export class PromptDialog {
  readonly data = inject<PromptDialogData>(MAT_DIALOG_DATA);
  readonly ref = inject(MatDialogRef<PromptDialog>);
  readonly value = signal(this.data.initial ?? '');
}
