# LUMI-notifications

Tools to send push notifications to various tools. These can then be used, e.g.,
at the start and end of job execution as there is no way to have Slurm send mails
to users.

Currently only two tools are included:

- pushover: Sent a notification to the [pushover service](https://pushover.net/).

- pushslack: Sent a notification to [Slack](https://slack.com/)

The code can be used as a template though for other similar services.
